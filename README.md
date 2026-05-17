# Hoshikuzu Emi
An all-in-one tournament/community management bot agent

Version Beta 1 Interim 6 (v. 0.5.6)

## Overview
This lightweight C++-based bot agent provides all-in-one management tailored to tournaments and e-sports-centric servers.

## Features
All supported features in the Core version are as follows:

- Username Registration System w/ Search
- Help Page
- Player Information Finder (Basic - 1.0.0) (SQLite-based)
- TETR.IO Profile System (Basic - 1.0.0)
- Internal Tree-based Bracket Management without the use of external APIs
- Moderation Feature
- Visualisation
- Module-based UI Language System & Multilingual Support (Korean)

## In development
- SVG Customisation
- Encryption Hardening
- etc...

## Licence
This service (Core version) is distributed under the MIT licence.

## 💡 Economic & Eco-Friendly Hosting Notice
The Core version of this service is designed to utilise your host machine's local resources. To minimise electricity costs and reduce your environmental footprint, we strongly recommend hosting the bot on a dedicated Raspberry Pi 5 (with a 256GB microSD/NVMe) running Ubuntu Server.

Running this service on a standard desktop PC 24/7 can be significantly more expensive (est. 10x higher power draw) compared to the Pi 5's ultra-efficient ~3,000 KRW/month operating cost.

**Please do note that all third-party libraries listed in `LICENSE.md` must be installed on the host server machine before initialisation for proper use.**

**Triangle.js (used for automated room creation) is not vendored in this repository by default. The optional bridge declares `@haelp/teto` as an npm dependency, and operators install it locally when enabling room automation.**



---

## 🔒 Data & Privacy
Hoshikuzu Emi stores only the minimum data required to operate:

- Discord IDs  
- Linked external usernames (e.g., TETR.IO)  
- Server configuration (roles, settings)

No personal or sensitive data is collected.

Data is stored locally on the bot host and is never sold or shared.

---

## ⚠️ Disclaimer
This bot is provided "as is" without guarantees of uptime or accuracy.  
Features may change during development.

