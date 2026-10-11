// web/types.ts
var ELEMENT_COLORS = {
  [0 /* None */]: "#888888",
  [1 /* Pyro */]: "#EF7938",
  [2 /* Hydro */]: "#4CC2F1",
  [3 /* Electro */]: "#AF8CF0",
  [4 /* Cryo */]: "#9FD6E0",
  [5 /* Anemo */]: "#75D2B2",
  [6 /* Geo */]: "#FAB632",
  [7 /* Dendro */]: "#A5C83B"
};
var ELEMENT_NAMES = {
  [0 /* None */]: "",
  [1 /* Pyro */]: "\u706B",
  [2 /* Hydro */]: "\u6C34",
  [3 /* Electro */]: "\u96F7",
  [4 /* Cryo */]: "\u51B0",
  [5 /* Anemo */]: "\u98CE",
  [6 /* Geo */]: "\u5CA9",
  [7 /* Dendro */]: "\u8349"
};
var REACTION_NAMES = {
  0: "",
  1: "\u84B8\u53D1",
  2: "\u84B8\u53D1",
  3: "\u878D\u5316",
  4: "\u878D\u5316",
  5: "\u8D85\u8F7D",
  6: "\u611F\u7535",
  7: "\u51BB\u7ED3",
  8: "\u8D85\u5BFC",
  9: "\u706B\u6269\u6563",
  10: "\u6C34\u6269\u6563",
  11: "\u96F7\u6269\u6563",
  12: "\u51B0\u6269\u6563",
  13: "\u7ED3\u6676",
  14: "\u71C3\u70E7",
  15: "\u7EFD\u653E"
};
function unpackGauge(u8) {
  return Math.max(0, Math.min(1, u8 / 255));
}

// web/render.ts
var Renderer = class {
  ctx;
  width;
  height;
  // 每个敌人本帧仍在飘字的反应列表（支持一帧多个反应堆叠显示）
  // 每个元素 { type, t }：type 为 ReactionType 枚举，t 为剩余显示时长 s
  enemyReactions = /* @__PURE__ */ new Map();
  // UI：左侧元素面板高亮的当前选中元素（默认 Pyro，和初始 spawn_player 一致）
  currentElem = 1 /* Pyro */;
  // 敌人贴图（水史莱姆），圆形裁剪后绘制
  enemySprite = null;
  spriteLoaded = false;
  // 玩家贴图（派蒙），圆形裁剪后绘制
  playerSprite = null;
  playerSpriteLoaded = false;
  // 设置当前选中的元素（按键按下时由 main.ts 调用，立即更新白框高亮）
  setCurrentElement(elem) {
    this.currentElem = elem;
  }
  constructor(canvas2) {
    this.ctx = canvas2.getContext("2d");
    this.width = canvas2.width;
    this.height = canvas2.height;
    this.enemySprite = new Image();
    this.enemySprite.onload = () => {
      this.spriteLoaded = true;
    };
    this.enemySprite.src = "assets/hydro_slime.png";
    this.playerSprite = new Image();
    this.playerSprite.onload = () => {
      this.playerSpriteLoaded = true;
    };
    this.playerSprite.src = "assets/paimon.png";
  }
  draw(commands) {
    const ctx = this.ctx;
    ctx.fillStyle = "#1a1a2e";
    ctx.fillRect(0, 0, this.width, this.height);
    ctx.strokeStyle = "rgba(255,255,255,0.04)";
    ctx.lineWidth = 1;
    for (let x = 0; x < this.width; x += 40) {
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, this.height);
      ctx.stroke();
    }
    for (let y = 0; y < this.height; y += 40) {
      ctx.beginPath();
      ctx.moveTo(0, y);
      ctx.lineTo(this.width, y);
      ctx.stroke();
    }
    const bullets = [];
    const enemies = [];
    const player = [];
    for (const c of commands) {
      if (c.entity_type === 2 /* Bullet */) bullets.push(c);
      else if (c.entity_type === 1 /* Enemy */) enemies.push(c);
      else player.push(c);
    }
    for (const c of bullets) this.drawBullet(ctx, c);
    for (const c of enemies) this.drawEnemy(ctx, c);
    for (const c of player) this.drawPlayer(ctx, c);
    this.drawUI(ctx);
  }
  // ===== 玩家（含朝向箭头） =====
  drawPlayer(ctx, cmd) {
    const { x, y, radius, r, g, b, a, facing_dx, facing_dy } = cmd;
    if (this.playerSpriteLoaded && this.playerSprite) {
      ctx.save();
      ctx.imageSmoothingEnabled = true;
      ctx.imageSmoothingQuality = "high";
      ctx.beginPath();
      ctx.arc(x, y, radius, 0, Math.PI * 2);
      ctx.closePath();
      ctx.clip();
      const size = radius * 2;
      ctx.drawImage(this.playerSprite, x - radius, y - radius, size, size);
      ctx.restore();
    } else {
      ctx.beginPath();
      ctx.arc(x, y, radius, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(${r},${g},${b},${a / 255})`;
      ctx.fill();
      ctx.strokeStyle = "#ffffff";
      ctx.lineWidth = 2;
      ctx.stroke();
    }
    if (facing_dx !== 0 || facing_dy !== 0) {
      const len = radius + 14;
      const tipX = x + facing_dx * len;
      const tipY = y + facing_dy * len;
      const perpX = -facing_dy;
      const perpY = facing_dx;
      const w = 6;
      ctx.beginPath();
      ctx.moveTo(tipX, tipY);
      ctx.lineTo(
        x + facing_dx * (radius + 4) + perpX * w,
        y + facing_dy * (radius + 4) + perpY * w
      );
      ctx.lineTo(
        x + facing_dx * (radius + 4) - perpX * w,
        y + facing_dy * (radius + 4) - perpY * w
      );
      ctx.closePath();
      ctx.fillStyle = "#ffffff";
      ctx.fill();
    }
  }
  // ===== 敌人（元素光环 × 2 + 元素量条 + 反应标签） =====
  drawEnemy(ctx, cmd) {
    const {
      x,
      y,
      radius,
      r,
      g,
      b,
      a,
      element1_type,
      element2_type,
      element1_gauge,
      element2_gauge,
      reaction_mask_lo,
      reaction_mask_hi
    } = cmd;
    const key = `enemy_${cmd.x}_${cmd.y}`;
    if (this.spriteLoaded && this.enemySprite) {
      ctx.save();
      ctx.imageSmoothingEnabled = true;
      ctx.imageSmoothingQuality = "high";
      ctx.beginPath();
      ctx.arc(x, y, radius, 0, Math.PI * 2);
      ctx.closePath();
      ctx.clip();
      const size = radius * 2;
      ctx.drawImage(this.enemySprite, x - radius, y - radius, size, size);
      ctx.restore();
    } else {
      ctx.beginPath();
      ctx.arc(x, y, radius, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(${r},${g},${b},${a / 255})`;
      ctx.fill();
      const sr = Math.max(0, Math.floor(r * 0.6));
      const sg = Math.max(0, Math.floor(g * 0.6));
      const sb = Math.max(0, Math.floor(b * 0.6));
      ctx.strokeStyle = `rgb(${sr},${sg},${sb})`;
      ctx.lineWidth = 1.5;
      ctx.stroke();
    }
    const g1 = unpackGauge(element1_gauge);
    const g2 = unpackGauge(element2_gauge);
    if (element1_type !== 0 && g1 > 0) {
      const color = ELEMENT_COLORS[element1_type] || "#888";
      ctx.beginPath();
      ctx.arc(x, y, radius + 5, 0, Math.PI * 2);
      ctx.strokeStyle = color;
      ctx.lineWidth = 2.5;
      ctx.globalAlpha = 0.55 + Math.min(1, g1) * 0.35;
      ctx.stroke();
      ctx.globalAlpha = 1;
    }
    if (element2_type !== 0 && g2 > 0) {
      const color = ELEMENT_COLORS[element2_type] || "#888";
      ctx.beginPath();
      ctx.arc(x, y, radius + 11, 0, Math.PI * 2);
      ctx.strokeStyle = color;
      ctx.lineWidth = 2;
      ctx.globalAlpha = 0.5 + Math.min(1, g2) * 0.3;
      ctx.setLineDash([4, 3]);
      ctx.stroke();
      ctx.setLineDash([]);
      ctx.globalAlpha = 1;
    }
    const barW = radius * 2.2;
    const barH = 5;
    const barX = x - barW / 2;
    const barY = y + radius + 10;
    ctx.fillStyle = "#222";
    ctx.fillRect(barX, barY, barW, barH);
    const has1 = element1_type !== 0;
    const has2 = element2_type !== 0;
    if (has1 && !has2) {
      const color = ELEMENT_COLORS[element1_type] || "#888";
      ctx.fillStyle = color;
      ctx.fillRect(barX, barY, barW * g1, barH);
      ctx.fillStyle = color;
      ctx.font = "bold 10px monospace";
      ctx.textAlign = "left";
      ctx.fillText(ELEMENT_NAMES[element1_type] || "", barX, barY - 1);
    } else if (has1 && has2) {
      const color1 = ELEMENT_COLORS[element1_type] || "#888";
      const color2 = ELEMENT_COLORS[element2_type] || "#888";
      ctx.fillStyle = color1;
      ctx.fillRect(barX, barY, barW / 2 * g1, barH);
      ctx.fillStyle = color2;
      ctx.fillRect(barX + barW / 2, barY, barW / 2 * g2, barH);
      ctx.font = "bold 10px monospace";
      ctx.textAlign = "left";
      ctx.fillStyle = color1;
      ctx.fillText(ELEMENT_NAMES[element1_type] || "", barX, barY - 1);
      ctx.textAlign = "right";
      ctx.fillStyle = color2;
      ctx.fillText(ELEMENT_NAMES[element2_type] || "", barX + barW, barY - 1);
    } else if (has2 && !has1) {
      const color = ELEMENT_COLORS[element2_type] || "#888";
      ctx.fillStyle = color;
      ctx.fillRect(barX, barY, barW * g2, barH);
    }
    const cached = this.enemyReactions.get(key) || { items: [] };
    const mask = reaction_mask_lo & 255 | (reaction_mask_hi & 255) << 8;
    for (let i = 1; i <= 16; i++) {
      if ((mask & 1 << i) !== 0) {
        cached.items.push({ type: i, t: 1.5 });
      }
    }
    const dtFrame = 1 / 60;
    cached.items = cached.items.map((it) => ({ ...it, t: it.t - dtFrame })).filter((it) => it.t > 0).sort((a2, b2) => b2.t - a2.t);
    if (cached.items.length > 0) {
      ctx.font = "bold 14px monospace";
      ctx.textAlign = "center";
      ctx.fillStyle = "#ffdd33";
      const lineHeight = 18;
      for (let i = 0; i < cached.items.length; i++) {
        const it = cached.items[i];
        const name = REACTION_NAMES[it.type] || "";
        if (!name) continue;
        ctx.globalAlpha = Math.min(1, it.t);
        const baseY = y - radius - 18;
        const rise = (1.5 - it.t) * 20;
        const offset = i * lineHeight;
        ctx.fillText(name, x, baseY - rise - offset);
      }
      ctx.globalAlpha = 1;
    }
    this.enemyReactions.set(key, cached);
  }
  // ===== 子弹 =====
  drawBullet(ctx, cmd) {
    const { x, y, radius, r, g, b, a, bullet_strength } = cmd;
    if (bullet_strength >= 2) {
      const prevComposite = ctx.globalCompositeOperation;
      ctx.globalCompositeOperation = "lighter";
      const bigGlow = ctx.createRadialGradient(x, y, 0, x, y, radius + 20);
      bigGlow.addColorStop(0, `rgba(${r},${g},${b},0.90)`);
      bigGlow.addColorStop(0.3, `rgba(${r},${g},${b},0.55)`);
      bigGlow.addColorStop(0.65, `rgba(${r},${g},${b},0.25)`);
      bigGlow.addColorStop(1, `rgba(${r},${g},${b},0)`);
      ctx.fillStyle = bigGlow;
      ctx.beginPath();
      ctx.arc(x, y, radius + 20, 0, Math.PI * 2);
      ctx.fill();
      const tongues = 8;
      const innerR = radius + 5;
      const outerR = radius + 15;
      const phase = (x * 0.13 + y * 0.07) % (Math.PI * 2);
      ctx.beginPath();
      for (let i = 0; i < tongues * 2; i++) {
        const ang = phase + i * Math.PI / tongues;
        const rr = i % 2 === 0 ? outerR : innerR;
        const px = x + Math.cos(ang) * rr;
        const py = y + Math.sin(ang) * rr;
        if (i === 0) ctx.moveTo(px, py);
        else ctx.lineTo(px, py);
      }
      ctx.closePath();
      const flameGrad = ctx.createRadialGradient(x, y, radius, x, y, outerR);
      flameGrad.addColorStop(0, `rgba(${r},${g},${b},0.90)`);
      flameGrad.addColorStop(1, `rgba(${r},${g},${b},0)`);
      ctx.fillStyle = flameGrad;
      ctx.fill();
      ctx.globalCompositeOperation = prevComposite;
      ctx.beginPath();
      ctx.arc(x, y, radius + 3, 0, Math.PI * 2);
      ctx.strokeStyle = `rgba(${r},${g},${b},1.0)`;
      ctx.lineWidth = 3;
      ctx.stroke();
    }
    const grad = ctx.createRadialGradient(x, y, 0, x, y, radius + 6);
    grad.addColorStop(0, `rgba(${r},${g},${b},220)`);
    grad.addColorStop(0.6, `rgba(${r},${g},${b},140)`);
    grad.addColorStop(1, "rgba(100,180,255,0)");
    ctx.fillStyle = grad;
    ctx.beginPath();
    ctx.arc(x, y, radius + 6, 0, Math.PI * 2);
    ctx.fill();
    ctx.beginPath();
    ctx.arc(x, y, radius, 0, Math.PI * 2);
    ctx.fillStyle = `rgba(${r},${g},${b},${a / 255})`;
    ctx.fill();
    if (bullet_strength >= 2) {
      ctx.beginPath();
      ctx.arc(x, y, radius * 0.42, 0, Math.PI * 2);
      ctx.fillStyle = "rgba(255,255,255,0.85)";
      ctx.fill();
    }
  }
  drawUI(ctx) {
    ctx.fillStyle = "rgba(255,255,255,0.75)";
    ctx.font = "13px monospace";
    ctx.textAlign = "left";
    const curElemName = ELEMENT_NAMES[this.currentElem] || "";
    ctx.fillText(`WASD: \u79FB\u52A8   \u9F20\u6807\u79FB\u52A8: \u6539\u53D8\u671D\u5411   \u9F20\u6807\u5DE6\u952E: \u53D1\u5C04${curElemName}(\u5F31)   \u9F20\u6807\u53F3\u952E: \u53D1\u5C04${curElemName}(\u5F3A\xB7\u5E26\u706B\u7130)`, 10, 22);
    ctx.fillText("\u6570\u5B57\u952E1-7: \u5207\u6362\u73A9\u5BB6\u5143\u7D20", 10, 40);
    const panelX = 14;
    const panelY = 70;
    const rowH = 28;
    const entries = [
      { key: "1", elem: 1 /* Pyro */ },
      { key: "2", elem: 2 /* Hydro */ },
      { key: "3", elem: 3 /* Electro */ },
      { key: "4", elem: 4 /* Cryo */ },
      { key: "5", elem: 5 /* Anemo */ },
      { key: "6", elem: 6 /* Geo */ },
      { key: "7", elem: 7 /* Dendro */ }
    ];
    ctx.font = "14px monospace";
    for (let i = 0; i < entries.length; i++) {
      const { key, elem } = entries[i];
      const y = panelY + i * rowH;
      const rowW = 142;
      const name = ELEMENT_NAMES[elem] || "";
      const color = ELEMENT_COLORS[elem] || "#888";
      if (elem === this.currentElem) {
        ctx.fillStyle = "rgba(255,255,255,0.08)";
        ctx.fillRect(panelX, y - 18, rowW, rowH - 2);
        ctx.strokeStyle = "#ffffff";
        ctx.lineWidth = 2;
        ctx.strokeRect(panelX, y - 18, rowW, rowH - 2);
      }
      ctx.fillStyle = "#ffffff";
      ctx.textAlign = "left";
      ctx.fillText(key, panelX + 8, y);
      ctx.fillStyle = "rgba(255,255,255,0.4)";
      ctx.fillText("-", panelX + 28, y);
      const sqX = panelX + 44;
      const sqY = y - 12;
      ctx.fillStyle = color;
      ctx.fillRect(sqX, sqY, 14, 14);
      ctx.strokeStyle = "rgba(255,255,255,0.5)";
      ctx.lineWidth = 1;
      ctx.strokeRect(sqX, sqY, 14, 14);
      ctx.fillStyle = color;
      ctx.fillText(name, panelX + 68, y);
    }
  }
};

// web/main.ts
var wasmModule;
var renderer;
var canvas;
var lastTime = 0;
var running = false;
var input = {
  moveUp: false,
  moveDown: false,
  moveLeft: false,
  moveRight: false,
  mouseX: 0,
  mouseY: 0,
  mouseClick: false,
  mouseRightClick: false,
  elemKey: 0
};
function onKeyDown(e) {
  switch (e.key.toLowerCase()) {
    case "w":
    case "arrowup":
      input.moveUp = true;
      break;
    case "s":
    case "arrowdown":
      input.moveDown = true;
      break;
    case "a":
    case "arrowleft":
      input.moveLeft = true;
      break;
    case "d":
    case "arrowright":
      input.moveRight = true;
      break;
    case "1":
      input.elemKey = 1;
      renderer.setCurrentElement(1 /* Pyro */);
      break;
    // 火
    case "2":
      input.elemKey = 2;
      renderer.setCurrentElement(2 /* Hydro */);
      break;
    // 水
    case "3":
      input.elemKey = 3;
      renderer.setCurrentElement(3 /* Electro */);
      break;
    // 雷
    case "4":
      input.elemKey = 4;
      renderer.setCurrentElement(4 /* Cryo */);
      break;
    // 冰
    case "5":
      input.elemKey = 5;
      renderer.setCurrentElement(5 /* Anemo */);
      break;
    // 风
    case "6":
      input.elemKey = 6;
      renderer.setCurrentElement(6 /* Geo */);
      break;
    // 岩
    case "7":
      input.elemKey = 7;
      renderer.setCurrentElement(7 /* Dendro */);
      break;
  }
}
function onKeyUp(e) {
  switch (e.key.toLowerCase()) {
    case "w":
    case "arrowup":
      input.moveUp = false;
      break;
    case "s":
    case "arrowdown":
      input.moveDown = false;
      break;
    case "a":
    case "arrowleft":
      input.moveLeft = false;
      break;
    case "d":
    case "arrowright":
      input.moveRight = false;
      break;
  }
}
function onMouseMove(e) {
  const rect = canvas.getBoundingClientRect();
  const scaleX = canvas.width / rect.width;
  const scaleY = canvas.height / rect.height;
  input.mouseX = (e.clientX - rect.left) * scaleX;
  input.mouseY = (e.clientY - rect.top) * scaleY;
}
function onMouseDown(e) {
  if (e.button === 0) input.mouseClick = true;
  if (e.button === 2) input.mouseRightClick = true;
}
function onMouseUp(e) {
  if (e.button === 0) input.mouseClick = false;
  if (e.button === 2) input.mouseRightClick = false;
}
function syncInput() {
  wasmModule._wasm_set_input(
    input.moveUp ? 1 : 0,
    input.moveDown ? 1 : 0,
    input.moveLeft ? 1 : 0,
    input.moveRight ? 1 : 0,
    input.mouseX,
    input.mouseY,
    input.mouseClick ? 1 : 0,
    input.mouseRightClick ? 1 : 0,
    input.elemKey
  );
  input.elemKey = 0;
}
var STRIDE = 32;
function readRenderCommands() {
  const count = wasmModule._wasm_get_render_count();
  if (count <= 0) return [];
  const ptr = wasmModule._wasm_get_render_data();
  const heap = wasmModule.HEAPU8;
  const f32 = new Float32Array(heap.buffer);
  const result = [];
  for (let i = 0; i < count; i++) {
    const base = ptr + i * STRIDE;
    result.push({
      x: f32[base / 4],
      y: f32[(base + 4) / 4],
      radius: f32[(base + 8) / 4],
      r: heap[base + 12],
      g: heap[base + 13],
      b: heap[base + 14],
      a: heap[base + 15],
      entity_type: heap[base + 16],
      element1_type: heap[base + 17],
      reaction_mask_lo: heap[base + 18],
      element2_type: heap[base + 19],
      element1_gauge: heap[base + 20],
      element2_gauge: heap[base + 21],
      bullet_strength: heap[base + 22],
      reaction_mask_hi: heap[base + 23],
      facing_dx: f32[(base + 24) / 4],
      facing_dy: f32[(base + 28) / 4]
    });
  }
  return result;
}
function gameLoop(timestamp) {
  if (!running) return;
  const dt = Math.min((timestamp - lastTime) / 1e3, 0.1);
  lastTime = timestamp;
  syncInput();
  wasmModule._wasm_update(dt);
  renderer.draw(readRenderCommands());
  requestAnimationFrame(gameLoop);
}
async function init() {
  canvas = document.getElementById("gameCanvas");
  if (!canvas) throw new Error("Canvas not found");
  canvas.width = 800;
  canvas.height = 600;
  renderer = new Renderer(canvas);
  wasmModule = await window.GameModule();
  wasmModule._wasm_init();
  renderer.setCurrentElement(1 /* Pyro */);
  window.addEventListener("keydown", onKeyDown);
  window.addEventListener("keyup", onKeyUp);
  canvas.addEventListener("mousemove", onMouseMove);
  canvas.addEventListener("mousedown", onMouseDown);
  window.addEventListener("mouseup", onMouseUp);
  canvas.addEventListener("contextmenu", (e) => e.preventDefault());
  document.getElementById("btnReset")?.addEventListener("click", () => {
    wasmModule._wasm_reset();
  });
  running = true;
  lastTime = performance.now();
  requestAnimationFrame(gameLoop);
  console.log("[GenshinElement] \u521D\u59CB\u5316\u5B8C\u6210");
  console.log("\u64CD\u4F5C: WASD \u79FB\u52A8 | \u9F20\u6807\u79FB\u52A8\u6539\u53D8\u671D\u5411 | \u9F20\u6807\u5DE6\u952E\u53D1\u5C04\u5B50\u5F39 | \u6570\u5B57\u952E1-7\u5207\u6362\u73A9\u5BB6\u5143\u7D20(1\u706B2\u6C343\u96F74\u51B05\u98CE6\u5CA97\u8349)");
}
init().catch((err) => console.error("Init failed:", err));
