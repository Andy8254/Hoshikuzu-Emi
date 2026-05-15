import { createServer } from "node:http";
import { mkdir, writeFile } from "node:fs/promises";
import { join } from "node:path";
import { Client } from "@haelp/teto";

const port = Number.parseInt(process.env.TRIANGLE_BRIDGE_PORT ?? "8787", 10);
const userAgent = process.env.TRIANGLE_USER_AGENT ?? "Hoshikuzu-Emi-Triangle-Bridge/0.2";
const replayDir = process.env.TRIANGLE_REPLAY_DIR ?? "db/replays/tetrio";
const roomPreset = process.env.TRIANGLE_ROOM_PRESET ?? "tetra league";
const fallbackFirstTo = Number.parseInt(process.env.TRIANGLE_FIRST_TO ?? "2", 10);
const fallbackStartGraceSeconds = Number.parseInt(process.env.TRIANGLE_MATCH_START_GRACE_SECONDS ?? "30", 10);
const fallbackWarmupMatches = Number.parseInt(process.env.TRIANGLE_WARMUP_MATCHES ?? "1", 10);
const fallbackPostWarmupStartDelaySeconds = Number.parseInt(process.env.TRIANGLE_POST_WARMUP_START_DELAY_SECONDS ?? "10", 10);
const maxActiveRooms = Number.parseInt(process.env.TRIANGLE_MAX_ACTIVE_ROOMS ?? "1", 10);
const debugEvents = isTruthy(process.env.TRIANGLE_DEBUG_EVENTS);

let clientPromise = null;
let debugAttached = false;
const activeRooms = new Map();

function isTruthy(value) {
  return value === "1" || value === "true" || value === "TRUE" || value === "yes" || value === "on";
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function readJson(request) {
  return new Promise((resolve, reject) => {
    let body = "";
    request.setEncoding("utf8");
    request.on("data", (chunk) => {
      body += chunk;
      if (body.length > 64 * 1024) {
        reject(new Error("Request body too large."));
        request.destroy();
      }
    });
    request.on("end", () => {
      try {
        resolve(body ? JSON.parse(body) : {});
      } catch {
        reject(new Error("Request body is not valid JSON."));
      }
    });
    request.on("error", reject);
  });
}

function sendJson(response, status, payload) {
  const body = JSON.stringify(payload);
  response.writeHead(status, {
    "content-type": "application/json; charset=utf-8",
    "content-length": Buffer.byteLength(body)
  });
  response.end(body);
}

async function triangleClient() {
  if (clientPromise) {
    return clientPromise;
  }

  const token = process.env.TETRIO_BOT_TOKEN;
  const username = process.env.TETRIO_BOT_USERNAME;
  const password = process.env.TETRIO_BOT_PASSWORD;

  if (token) {
    clientPromise = Client.create({ token, userAgent });
  } else if (username && password) {
    clientPromise = Client.create({ username, password, userAgent });
  } else {
    throw new Error("Set TETRIO_BOT_TOKEN or TETRIO_BOT_USERNAME/TETRIO_BOT_PASSWORD.");
  }

  const client = await clientPromise;
  attachDebugEvents(client);
  return client;
}

function attachDebugEvents(client) {
  if (!debugEvents || debugAttached || typeof client.on !== "function") {
    return;
  }

  debugAttached = true;
  for (const eventName of [
    "client.ready",
    "client.error",
    "client.fail",
    "client.room.players",
    "client.game.start",
    "client.game.end",
    "client.game.over"
  ]) {
    client.on(eventName, (payload) => {
      console.log(`[triangle:${eventName}]`, payload ?? "");
    });
  }
}

function roomUrl(roomId) {
  return `https://tetr.io/#R:${roomId}`;
}

function safeFilenamePart(value) {
  return String(value ?? "unknown")
    .trim()
    .replace(/[^a-zA-Z0-9._-]+/g, "_")
    .replace(/^_+|_+$/g, "")
    .slice(0, 80) || "unknown";
}

function playerNames(payload) {
  return [payload.player_a_name, payload.player_b_name]
    .map((value) => String(value ?? "").trim())
    .filter((value) => value.length > 0);
}

function numericOption(value, fallback, minimum = 0) {
  const parsed = Number.parseInt(String(value ?? ""), 10);
  if (!Number.isFinite(parsed) || Number.isNaN(parsed)) {
    return fallback;
  }
  return Math.max(minimum, parsed);
}

async function maybeChat(room, message, pinned = false) {
  try {
    if (typeof room.chat === "function") {
      await room.chat(message, pinned);
    }
  } catch (error) {
    console.warn("[triangle] room chat failed:", error instanceof Error ? error.message : error);
  }
}

async function setRoomFirstTo(room, firstTo) {
  if (typeof room.update === "function") {
    await room.update({ index: "match.ft", value: firstTo });
  }
}

async function configureRoom(room, payload, players, state) {
  const startGraceSeconds = numericOption(payload.start_grace_seconds, fallbackStartGraceSeconds, 0);
  const initialFirstTo = state.warmupMatches > 0 ? 1 : state.officialFirstTo;

  if (typeof room.switch === "function") {
    await room.switch("spectator");
  }

  if (typeof room.usePreset === "function") {
    await room.usePreset(String(payload.preset ?? roomPreset));
  }

  await setRoomFirstTo(room, initialFirstTo);

  await maybeChat(
    room,
    `Hoshikuzu match ${payload.match_id ?? "unknown"} ready: ${players[0]} vs ${players[1]}. Invite grace: ${startGraceSeconds}s. Warm-up: ${state.warmupMatches > 0 ? `FT1 x${state.warmupMatches}` : "off"}. Official FT: ${state.officialFirstTo}.`,
    true
  );
}

async function resolveAndInvitePlayers(client, players) {
  if (!client.social || typeof client.social.resolve !== "function" || typeof client.social.invite !== "function") {
    throw new Error("Triangle client does not expose social.resolve/social.invite.");
  }

  const invited = [];
  for (const name of players) {
    const id = await client.social.resolve(name);
    await client.social.invite(id);
    invited.push({ name, id });
    await sleep(300);
  }
  return invited;
}

function attachReplayCapture(client, room, state) {
  if (typeof client.on !== "function") {
    throw new Error("Triangle client does not expose event handling.");
  }

  const saveReplay = async (reason) => {
    if (state.replaySaved) {
      return;
    }

    await sleep(1000);
    if (!room.replay || typeof room.replay.export !== "function") {
      console.warn(`[triangle] replay unavailable for match ${state.matchId} after ${reason}`);
      return;
    }

    const replay = room.replay.export();
    await mkdir(replayDir, { recursive: true });
    const timestamp = new Date().toISOString().replace(/[:.]/g, "-");
    const playersPart = state.players.map(safeFilenamePart).join("_vs_");
    const filename = `${timestamp}_t${safeFilenamePart(state.tournamentId)}_m${safeFilenamePart(state.matchId)}_${playersPart}_game${state.gameNumber}.ttrm`;
    const path = join(replayDir, filename);
    await writeFile(path, JSON.stringify(replay, null, 2), "utf8");

    state.replaySaved = true;
    state.replayPath = path;
    await maybeChat(room, `Replay saved locally: ${filename}`);
    cleanup();
  };

  const onGameStart = async () => {
    if (state.phase === "official") {
      state.gameNumber += 1;
      state.replaySaved = false;
    }

    await sleep(500);
    if (typeof room.spectate === "function") {
      await room.spectate().catch((error) => {
        console.warn("[triangle] room spectate failed:", error instanceof Error ? error.message : error);
      });
    }
  };

  const onGameEnd = () => {
    handleGameEnd("client.game.end").catch((error) => {
      console.error("[triangle] game-end handling failed:", error instanceof Error ? error.message : error);
    });
  };

  const onGameOver = () => {
    handleGameEnd("client.game.over").catch((error) => {
      console.error("[triangle] game-over handling failed:", error instanceof Error ? error.message : error);
    });
  };

  const handleGameEnd = async (reason) => {
    if (state.phase === "warmup") {
      state.completedWarmups += 1;
      if (state.completedWarmups < state.warmupMatches) {
        await maybeChat(room, `Warm-up ${state.completedWarmups}/${state.warmupMatches} complete. Next FT1 warm-up starts in ${state.postWarmupStartDelaySeconds} seconds.`);
        state.phase = "warmup_pending";
        scheduleStart(client, room, state, state.postWarmupStartDelaySeconds, "warmup");
        return;
      }

      await setRoomFirstTo(room, state.officialFirstTo);
      await maybeChat(room, `Warm-up complete. Official FT${state.officialFirstTo} starts in ${state.postWarmupStartDelaySeconds} seconds.`);
      state.phase = "official_pending";
      scheduleStart(client, room, state, state.postWarmupStartDelaySeconds, "official");
      return;
    }

    if (state.phase === "official") {
      await saveReplay(reason);
    }
  };

  const cleanup = () => {
    activeRooms.delete(state.key);
    if (typeof client.off === "function") {
      client.off("client.game.start", onGameStart);
      client.off("client.game.end", onGameEnd);
      client.off("client.game.over", onGameOver);
    }
  };

  client.on("client.game.start", onGameStart);
  client.on("client.game.end", onGameEnd);
  client.on("client.game.over", onGameOver);
  state.cleanup = cleanup;
}

async function startMatch(client, room, state, phase) {
  try {
    if (room.self?.bracket !== "spectator" && typeof room.switch === "function") {
      await room.switch("spectator");
    }

    if (typeof room.start !== "function") {
      throw new Error("Triangle room does not expose start().");
    }

    state.phase = phase;
    await room.start();
    if (client.game && typeof client.game.spectate === "function") {
      await client.game.spectate("all");
    }
    if (phase === "warmup") {
      await maybeChat(room, `FT1 warm-up started (${state.completedWarmups + 1}/${state.warmupMatches}).`);
    } else {
      await maybeChat(room, "Official match started. Replay capture is armed.");
    }
  } catch (error) {
    state.error = error instanceof Error ? error.message : String(error);
    await maybeChat(room, `Automatic ${phase} start failed: ${state.error}`);
    state.cleanup?.();
  }
}

function scheduleStart(client, room, state, seconds, phase) {
  state.startTimer = setTimeout(() => {
    startMatch(client, room, state, phase).catch((error) => {
      state.error = error instanceof Error ? error.message : String(error);
      state.cleanup?.();
    });
  }, seconds * 1000);
}

async function createRoom(payload) {
  const players = playerNames(payload);
  if (players.length !== 2) {
    throw new Error("player_a_name and player_b_name are required for Triangle invites.");
  }

  if (activeRooms.size >= maxActiveRooms) {
    throw new Error(`Triangle bridge active room limit reached (${maxActiveRooms}).`);
  }

  const client = await triangleClient();
  const room = await client.rooms.create("private");
  const key = String(payload.match_id ?? room.id);
  const startGraceSeconds = numericOption(payload.start_grace_seconds, fallbackStartGraceSeconds, 0);
  const officialFirstTo = numericOption(payload.first_to, fallbackFirstTo, 1);
  const warmupMatches = numericOption(payload.warmup_matches, fallbackWarmupMatches, 0);
  const postWarmupStartDelay = numericOption(payload.post_warmup_start_delay_seconds, fallbackPostWarmupStartDelaySeconds, 0);
  const firstPhase = warmupMatches > 0 ? "warmup" : "official";
  const state = {
    key,
    roomId: room.id,
    tournamentId: payload.tournament_id ?? "unknown",
    matchId: payload.match_id ?? "unknown",
    players,
    officialFirstTo,
    warmupMatches,
    postWarmupStartDelaySeconds: postWarmupStartDelay,
    completedWarmups: 0,
    gameNumber: 0,
    replaySaved: false,
    replayPath: "",
    error: "",
    phase: "pending"
  };

  activeRooms.set(key, state);

  try {
    await configureRoom(room, payload, players, state);
    const invited = await resolveAndInvitePlayers(client, players);
    if (warmupMatches > 0) {
      await maybeChat(room, `FT1 warm-up starts in ${startGraceSeconds} seconds.`);
    }
    attachReplayCapture(client, room, state);
    scheduleStart(client, room, state, startGraceSeconds, firstPhase);

    return {
      ok: true,
      room_id: room.id,
      room_url: roomUrl(room.id),
      invited,
      invite_grace_seconds: startGraceSeconds,
      warmup_matches: warmupMatches,
      post_warmup_start_delay_seconds: postWarmupStartDelay,
      official_first_to: officialFirstTo,
      start_in_seconds: startGraceSeconds,
      replay_dir: replayDir
    };
  } catch (error) {
    state.cleanup?.();
    activeRooms.delete(key);
    throw error;
  }
}

const server = createServer(async (request, response) => {
  try {
    if (request.method === "GET" && request.url === "/health") {
      sendJson(response, 200, {
        ok: true,
        active_rooms: activeRooms.size,
        max_active_rooms: maxActiveRooms
      });
      return;
    }

    if (request.method !== "POST" || request.url !== "/rooms") {
      sendJson(response, 404, { ok: false, error: "Not found." });
      return;
    }

    const payload = await readJson(request);
    if (!payload.match_id || !payload.player_a_id || !payload.player_b_id) {
      sendJson(response, 400, { ok: false, error: "match_id, player_a_id, and player_b_id are required." });
      return;
    }

    sendJson(response, 200, await createRoom(payload));
  } catch (error) {
    sendJson(response, 500, {
      ok: false,
      error: error instanceof Error ? error.message : "Unknown bridge error."
    });
  }
});

server.listen(port, "127.0.0.1", () => {
  console.log(`Triangle bridge listening on http://127.0.0.1:${port}`);
});
