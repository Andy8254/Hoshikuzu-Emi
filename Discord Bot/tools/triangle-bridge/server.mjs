import { createServer } from "node:http";
import { Client } from "@haelp/teto";

const port = Number.parseInt(process.env.TRIANGLE_BRIDGE_PORT ?? "8787", 10);
const userAgent = process.env.TRIANGLE_USER_AGENT ?? "Hoshikuzu-Emi-Triangle-Bridge/0.1";

let clientPromise = null;

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
    return clientPromise;
  }

  if (username && password) {
    clientPromise = Client.create({ username, password, userAgent });
    return clientPromise;
  }

  throw new Error("Set TETRIO_BOT_TOKEN or TETRIO_BOT_USERNAME/TETRIO_BOT_PASSWORD.");
}

function roomUrl(roomId) {
  return `https://tetr.io/#R:${roomId}`;
}

async function createRoom(payload) {
  const client = await triangleClient();
  const room = await client.rooms.create("private");

  const title = `Hoshikuzu match ${payload.match_id ?? "unknown"}`;
  try {
    if (typeof room.chat === "function") {
      room.chat(`${title} - players: ${payload.player_a_id ?? "A"} vs ${payload.player_b_id ?? "B"}`, true);
    }
  } catch {
    // Room creation succeeded; pinned/chat message failure should not fail the bridge request.
  }

  return {
    ok: true,
    room_id: room.id,
    room_url: roomUrl(room.id)
  };
}

const server = createServer(async (request, response) => {
  try {
    if (request.method === "GET" && request.url === "/health") {
      sendJson(response, 200, { ok: true });
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
