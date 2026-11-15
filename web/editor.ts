// =====================================================================
//  关卡编辑器 —— 独立于游戏页面，不加载 wasm
//  数据格式与 web/levels/levels.json 完全一致：
//    level_id, is_boss, player_spawn{x,y}, walls[{x,y,w,h}],
//    waves[{delay, spawns:[{preset_id,x,y}]}]
//  所有坐标归一化到 0~1（存比例，画布缩放不影响数据）
// =====================================================================

const WORLD_W = 1000;
const WORLD_H = 700;
const GRID_SIZE = 50; // 网格辅助线间距（像素）

// preset_id → 颜色（与 SpawnPreset 中 EnemyPreset.element 对应）
const PRESET_COLORS: Record<number, string> = {
  0: '#EF7938', // Pyro 火
  1: '#4CC2F1', // Hydro 水
  2: '#9FD6E0', // Cryo 冰
  3: '#AF8CF0', // Boss 雷
};
const PRESET_NAMES: Record<number, string> = {
  0: '火',
  1: '水',
  2: '冰',
  3: 'Boss',
};
const PRESET_RADIUS: Record<number, number> = {
  0: 20, 1: 20, 2: 22, 3: 36,
};

// ===== 数据类型（与 levels.json 对齐）=====
interface Wall { x: number; y: number; w: number; h: number; }
interface EnemySpawn { preset_id: number; x: number; y: number; }
interface Wave { delay: number; spawns: EnemySpawn[]; }
interface Level {
  level_id: number;
  is_boss: boolean;
  player_spawn: { x: number; y: number };
  walls: Wall[];
  waves: Wave[];
}

// 选中目标
type Selection =
  | { kind: 'wall'; index: number }
  | { kind: 'enemy'; waveIndex: number; spawnIndex: number }
  | { kind: 'spawn' }
  | null;

type Tool = 'select' | 'wall' | 'enemy' | 'spawn' | 'delete';

// ===== 全局状态 =====
let levels: Level[] = [];
let currentLevelIndex = 0;
let currentTool: Tool = 'select';
let currentPreset = 0;
let activeWaveIndex = 0;
let selection: Selection = null;

let isDragging = false;
let dragStart: { x: number; y: number } | null = null;
let dragCurrent: { x: number; y: number } | null = null;
let dragMoved = false;

// ===== DOM 引用 =====
const canvas = document.getElementById('editorCanvas') as HTMLCanvasElement;
const ctx = canvas.getContext('2d')!;
const statusBar = document.getElementById('statusBar')!;

// ===== 坐标转换 =====
// 画布像素 → 归一化（0~1）
// 注意：游戏渲染端直接把世界坐标当作 canvas 坐标使用（render.ts 未做 Y 翻转），
// 因此归一化 y=0 对应画布顶部，y=1 对应画布底部，与 canvas 坐标系一致，无需翻转。
function pixelToNorm(px: number, py: number): { x: number; y: number } {
  return { x: px / WORLD_W, y: py / WORLD_H };
}
// 归一化 → 画布像素
function normToPixel(nx: number, ny: number): { x: number; y: number } {
  return { x: nx * WORLD_W, y: ny * WORLD_H };
}

// ===== 工具函数：圆/矩形命中检测 =====
function pointInRect(px: number, py: number, cx: number, cy: number, w: number, h: number): boolean {
  return px >= cx - w / 2 && px <= cx + w / 2 && py >= cy - h / 2 && py <= cy + h / 2;
}
function pointInCircle(px: number, py: number, cx: number, cy: number, r: number): boolean {
  const dx = px - cx, dy = py - cy;
  return dx * dx + dy * dy <= r * r;
}

// ===== 获取当前关卡 =====
function curLevel(): Level {
  return levels[currentLevelIndex];
}

// ===== 渲染 =====
function render(): void {
  ctx.clearRect(0, 0, WORLD_W, WORLD_H);

  // 背景
  ctx.fillStyle = '#1a1a2e';
  ctx.fillRect(0, 0, WORLD_W, WORLD_H);

  // 网格
  ctx.strokeStyle = 'rgba(255,255,255,0.06)';
  ctx.lineWidth = 1;
  for (let x = 0; x <= WORLD_W; x += GRID_SIZE) {
    ctx.beginPath();
    ctx.moveTo(x + 0.5, 0);
    ctx.lineTo(x + 0.5, WORLD_H);
    ctx.stroke();
  }
  for (let y = 0; y <= WORLD_H; y += GRID_SIZE) {
    ctx.beginPath();
    ctx.moveTo(0, y + 0.5);
    ctx.lineTo(WORLD_W, y + 0.5);
    ctx.stroke();
  }

  // 边界
  ctx.strokeStyle = 'rgba(255,255,255,0.15)';
  ctx.lineWidth = 2;
  ctx.strokeRect(1, 1, WORLD_W - 2, WORLD_H - 2);

  const lvl = curLevel();

  // 墙体
  lvl.walls.forEach((w, i) => {
    const c = normToPixel(w.x, w.y);
    const pw = w.w * WORLD_W;
    const ph = w.h * WORLD_H;
    const isSel = selection?.kind === 'wall' && selection.index === i;
    ctx.fillStyle = isSel ? 'rgba(136,204,255,0.4)' : 'rgba(120,120,130,0.6)';
    ctx.fillRect(c.x - pw / 2, c.y - ph / 2, pw, ph);
    ctx.strokeStyle = isSel ? '#88ccff' : '#888';
    ctx.lineWidth = isSel ? 2 : 1;
    ctx.strokeRect(c.x - pw / 2, c.y - ph / 2, pw, ph);
  });

  // 敌人（按波次着色外圈）
  lvl.waves.forEach((wave, wi) => {
    const active = wi === activeWaveIndex;
    wave.spawns.forEach((sp, si) => {
      const c = normToPixel(sp.x, sp.y);
      const r = PRESET_RADIUS[sp.preset_id] ?? 20;
      const isSel = selection?.kind === 'enemy' && selection.waveIndex === wi && selection.spawnIndex === si;

      // 波次外圈（非当前波次淡显）
      ctx.globalAlpha = active ? 1 : 0.45;
      ctx.beginPath();
      ctx.arc(c.x, c.y, r + 4, 0, Math.PI * 2);
      ctx.strokeStyle = isSel ? '#88ccff' : `hsl(${(wi * 60) % 360}, 50%, 55%)`;
      ctx.lineWidth = isSel ? 3 : 2;
      ctx.stroke();

      // 敌人本体
      ctx.beginPath();
      ctx.arc(c.x, c.y, r, 0, Math.PI * 2);
      ctx.fillStyle = PRESET_COLORS[sp.preset_id] ?? '#888';
      ctx.fill();
      ctx.strokeStyle = isSel ? '#fff' : 'rgba(255,255,255,0.3)';
      ctx.lineWidth = isSel ? 2 : 1;
      ctx.stroke();

      // preset 名
      ctx.fillStyle = '#fff';
      ctx.font = '10px sans-serif';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(PRESET_NAMES[sp.preset_id] ?? '?', c.x, c.y - r - 10);

      ctx.globalAlpha = 1;
    });
  });

  // 玩家出生点
  const sc = normToPixel(lvl.player_spawn.x, lvl.player_spawn.y);
  const isSpawnSel = selection?.kind === 'spawn';
  drawStar(sc.x, sc.y, 16, 7, isSpawnSel ? '#88ccff' : '#ffd700');
  ctx.fillStyle = '#000';
  ctx.font = 'bold 11px sans-serif';
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText('P', sc.x, sc.y);

  // 画墙预览（拖拽中）
  if (currentTool === 'wall' && isDragging && dragStart && dragCurrent) {
    const x1 = Math.min(dragStart.x, dragCurrent.x);
    const y1 = Math.min(dragStart.y, dragCurrent.y);
    const w = Math.abs(dragCurrent.x - dragStart.x);
    const h = Math.abs(dragCurrent.y - dragStart.y);
    ctx.fillStyle = 'rgba(136,204,255,0.2)';
    ctx.fillRect(x1, y1, w, h);
    ctx.strokeStyle = '#88ccff';
    ctx.lineWidth = 1.5;
    ctx.strokeRect(x1, y1, w, h);
  }
}

function drawStar(cx: number, cy: number, outerR: number, innerR: number, color: string): void {
  ctx.beginPath();
  for (let i = 0; i < 10; i++) {
    const r = i % 2 === 0 ? outerR : innerR;
    const ang = (Math.PI / 5) * i - Math.PI / 2;
    const x = cx + Math.cos(ang) * r;
    const y = cy + Math.sin(ang) * r;
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }
  ctx.closePath();
  ctx.fillStyle = color;
  ctx.fill();
  ctx.strokeStyle = '#fff';
  ctx.lineWidth = 1.5;
  ctx.stroke();
}

// ===== 命中检测：找到点击位置的元素 =====
function hitTest(px: number, py: number): Selection {
  const lvl = curLevel();
  // 优先检测敌人（小目标放前面）
  for (let wi = lvl.waves.length - 1; wi >= 0; wi--) {
    const wave = lvl.waves[wi];
    for (let si = wave.spawns.length - 1; si >= 0; si--) {
      const sp = wave.spawns[si];
      const c = normToPixel(sp.x, sp.y);
      const r = PRESET_RADIUS[sp.preset_id] ?? 20;
      if (pointInCircle(px, py, c.x, c.y, r + 6)) {
        return { kind: 'enemy', waveIndex: wi, spawnIndex: si };
      }
    }
  }
  // 墙体
  for (let i = lvl.walls.length - 1; i >= 0; i--) {
    const w = lvl.walls[i];
    const c = normToPixel(w.x, w.y);
    if (pointInRect(px, py, c.x, c.y, w.w * WORLD_W, w.h * WORLD_H)) {
      return { kind: 'wall', index: i };
    }
  }
  // 出生点
  const sc = normToPixel(lvl.player_spawn.x, lvl.player_spawn.y);
  if (pointInCircle(px, py, sc.x, sc.y, 20)) {
    return { kind: 'spawn' };
  }
  return null;
}

// ===== 鼠标事件 =====
function getCanvasPos(e: MouseEvent): { x: number; y: number } {
  const rect = canvas.getBoundingClientRect();
  return {
    x: (e.clientX - rect.left) * (canvas.width / rect.width),
    y: (e.clientY - rect.top) * (canvas.height / rect.height),
  };
}

canvas.addEventListener('mousedown', (e) => {
  if (e.button !== 0) return; // 仅左键
  const pos = getCanvasPos(e);
  dragStart = pos;
  dragCurrent = pos;
  dragMoved = false;
  isDragging = true;

  if (currentTool === 'select') {
    selection = hitTest(pos.x, pos.y);
    updateSelProps();
    render();
  } else if (currentTool === 'delete') {
    const hit = hitTest(pos.x, pos.y);
    if (hit) deleteSelection(hit);
  } else if (currentTool === 'enemy') {
    placeEnemy(pos.x, pos.y);
  } else if (currentTool === 'spawn') {
    const n = pixelToNorm(pos.x, pos.y);
    curLevel().player_spawn = { x: round4(n.x), y: round4(n.y) };
    selection = { kind: 'spawn' };
    updateSelProps();
    render();
  }
  // wall 工具在 mousemove/mouseup 处理
});

canvas.addEventListener('mousemove', (e) => {
  const pos = getCanvasPos(e);
  if (!isDragging) return;
  dragCurrent = pos;
  if (Math.abs(pos.x - dragStart!.x) > 3 || Math.abs(pos.y - dragStart!.y) > 3) {
    dragMoved = true;
  }

  if (currentTool === 'select' && selection && dragMoved) {
    moveSelection(pos.x, pos.y);
  }
  if (currentTool === 'wall') render();
});

canvas.addEventListener('mouseup', (e) => {
  if (!isDragging) return;
  const pos = getCanvasPos(e);

  if (currentTool === 'wall' && dragMoved && dragStart) {
    createWall(dragStart.x, dragStart.y, pos.x, pos.y);
  }

  isDragging = false;
  dragStart = null;
  dragCurrent = null;
  dragMoved = false;
  render();
});

canvas.addEventListener('contextmenu', (e) => {
  e.preventDefault();
  // 右键删除
  const pos = getCanvasPos(e);
  const hit = hitTest(pos.x, pos.y);
  if (hit) {
    deleteSelection(hit);
  }
});

// ===== 工具操作 =====
function placeEnemy(px: number, py: number): void {
  const lvl = curLevel();
  if (lvl.waves.length === 0) {
    lvl.waves.push({ delay: 1.0, spawns: [] });
    activeWaveIndex = 0;
    refreshWaveList();
  }
  const n = pixelToNorm(px, py);
  const sp: EnemySpawn = {
    preset_id: currentPreset,
    x: round4(n.x),
    y: round4(n.y),
  };
  lvl.waves[activeWaveIndex].spawns.push(sp);
  selection = { kind: 'enemy', waveIndex: activeWaveIndex, spawnIndex: lvl.waves[activeWaveIndex].spawns.length - 1 };
  updateSelProps();
  render();
}

function createWall(x1: number, y1: number, x2: number, y2: number): void {
  const cx = (x1 + x2) / 2;
  const cy = (y1 + y2) / 2;
  const w = Math.abs(x2 - x1);
  const h = Math.abs(y2 - y1);
  if (w < 5 || h < 5) return; // 忽略过小的墙
  const n = pixelToNorm(cx, cy);
  const wall: Wall = {
    x: round4(n.x),
    y: round4(n.y),
    w: round4(w / WORLD_W),
    h: round4(h / WORLD_H),
  };
  curLevel().walls.push(wall);
  selection = { kind: 'wall', index: curLevel().walls.length - 1 };
  updateSelProps();
  render();
}

function moveSelection(px: number, py: number): void {
  const lvl = curLevel();
  const n = pixelToNorm(px, py);
  if (selection?.kind === 'wall') {
    lvl.walls[selection.index].x = round4(n.x);
    lvl.walls[selection.index].y = round4(n.y);
  } else if (selection?.kind === 'enemy') {
    lvl.waves[selection.waveIndex].spawns[selection.spawnIndex].x = round4(n.x);
    lvl.waves[selection.waveIndex].spawns[selection.spawnIndex].y = round4(n.y);
  } else if (selection?.kind === 'spawn') {
    lvl.player_spawn = { x: round4(n.x), y: round4(n.y) };
  }
  updateSelProps();
  render();
}

function deleteSelection(sel: Selection): void {
  if (!sel) return;
  const lvl = curLevel();
  if (sel.kind === 'wall') {
    lvl.walls.splice(sel.index, 1);
  } else if (sel.kind === 'enemy') {
    lvl.waves[sel.waveIndex].spawns.splice(sel.spawnIndex, 1);
  } else if (sel.kind === 'spawn') {
    // 出生点不能真正删除，重置到默认
    lvl.player_spawn = { x: 0.15, y: 0.5 };
  }
  if (selection === sel) selection = null;
  updateSelProps();
  render();
}

function round4(v: number): number {
  return Math.round(v * 10000) / 10000;
}

// ===== 属性面板 =====
function updateSelProps(): void {
  const div = document.getElementById('selProps')!;
  if (!selection) {
    div.innerHTML = '<div class="hint">未选中任何元素<br>点击画布中的墙/敌人/出生点进行编辑</div>';
    return;
  }
  const lvl = curLevel();
  let html = '';

  if (selection.kind === 'wall') {
    const w = lvl.walls[selection.index];
    html = `
      <div class="prop-row"><label>x（中心）</label><input type="number" step="0.01" min="0" max="1" data-field="x" value="${w.x}"></div>
      <div class="prop-row"><label>y（中心）</label><input type="number" step="0.01" min="0" max="1" data-field="y" value="${w.y}"></div>
      <div class="prop-row"><label>宽 w</label><input type="number" step="0.01" min="0" max="1" data-field="w" value="${w.w}"></div>
      <div class="prop-row"><label>高 h</label><input type="number" step="0.01" min="0" max="1" data-field="h" value="${w.h}"></div>
      <button class="btn btn-danger btn-sm" id="btnDelSel" style="margin-top:8px;">删除此墙</button>
    `;
  } else if (selection.kind === 'enemy') {
    const sp = lvl.waves[selection.waveIndex].spawns[selection.spawnIndex];
    html = `
      <div class="prop-row"><label>preset_id</label>
        <select data-field="preset_id">
          <option value="0" ${sp.preset_id === 0 ? 'selected' : ''}>火</option>
          <option value="1" ${sp.preset_id === 1 ? 'selected' : ''}>水</option>
          <option value="2" ${sp.preset_id === 2 ? 'selected' : ''}>冰</option>
          <option value="3" ${sp.preset_id === 3 ? 'selected' : ''}>Boss</option>
        </select>
      </div>
      <div class="prop-row"><label>x</label><input type="number" step="0.01" min="0" max="1" data-field="x" value="${sp.x}"></div>
      <div class="prop-row"><label>y</label><input type="number" step="0.01" min="0" max="1" data-field="y" value="${sp.y}"></div>
      <div class="hint" style="margin-top:6px;">所属波次：第 ${selection.waveIndex + 1} 波</div>
      <button class="btn btn-danger btn-sm" id="btnDelSel" style="margin-top:8px;">删除此敌人</button>
    `;
  } else if (selection.kind === 'spawn') {
    const ps = lvl.player_spawn;
    html = `
      <div class="prop-row"><label>x</label><input type="number" step="0.01" min="0" max="1" data-field="x" value="${ps.x}"></div>
      <div class="prop-row"><label>y</label><input type="number" step="0.01" min="0" max="1" data-field="y" value="${ps.y}"></div>
      <div class="hint" style="margin-top:6px;">每关唯一，重置将回到 (0.15, 0.5)</div>
      <button class="btn btn-danger btn-sm" id="btnDelSel" style="margin-top:8px;">重置出生点</button>
    `;
  }

  div.innerHTML = html;

  // 绑定字段修改
  div.querySelectorAll<HTMLInputElement | HTMLSelectElement>('[data-field]').forEach(inp => {
    inp.addEventListener('input', () => {
      if (selection!.kind === 'wall') {
        const w = lvl.walls[(selection as any).index];
        const f = inp.dataset.field!;
        (w as any)[f] = parseFloat(inp.value) || 0;
      } else if (selection!.kind === 'enemy') {
        const sp = lvl.waves[(selection as any).waveIndex].spawns[(selection as any).spawnIndex];
        const f = inp.dataset.field!;
        (sp as any)[f] = f === 'preset_id' ? parseInt(inp.value, 10) : parseFloat(inp.value) || 0;
      } else if (selection!.kind === 'spawn') {
        const f = inp.dataset.field!;
        (lvl.player_spawn as any)[f] = parseFloat(inp.value) || 0;
      }
      render();
    });
  });

  const delBtn = document.getElementById('btnDelSel');
  if (delBtn) delBtn.addEventListener('click', () => deleteSelection(selection));
}

// ===== 波次列表 =====
function refreshWaveList(): void {
  const list = document.getElementById('waveList')!;
  const lvl = curLevel();
  list.innerHTML = '';
  lvl.waves.forEach((w, i) => {
    const item = document.createElement('div');
    item.className = 'wave-item' + (i === activeWaveIndex ? ' active' : '');
    item.innerHTML = `
      <span class="wave-name">第 ${i + 1} 波（${w.spawns.length} 怪）</span>
      <input type="number" step="0.1" min="0" value="${w.delay}" style="width:50px;background:#141428;color:#ddd;border:1px solid #444;border-radius:3px;padding:2px 4px;font-size:11px;" data-wave-delay="${i}">
      <span class="wave-del" data-wave-del="${i}">✕</span>
    `;
    item.addEventListener('click', (e) => {
      if ((e.target as HTMLElement).dataset.waveDel !== undefined) return;
      if ((e.target as HTMLElement).dataset.waveDelay !== undefined) return;
      activeWaveIndex = i;
      refreshWaveList();
      render();
    });
    list.appendChild(item);
  });

  // delay 输入
  list.querySelectorAll<HTMLInputElement>('[data-wave-delay]').forEach(inp => {
    inp.addEventListener('input', () => {
      const idx = parseInt(inp.dataset.waveDelay!, 10);
      lvl.waves[idx].delay = parseFloat(inp.value) || 0;
    });
  });
  // 删除波次
  list.querySelectorAll('[data-wave-del]').forEach(el => {
    el.addEventListener('click', (e) => {
      e.stopPropagation();
      const idx = parseInt((el as HTMLElement).dataset.waveDel!, 10);
      lvl.waves.splice(idx, 1);
      if (activeWaveIndex >= lvl.waves.length) activeWaveIndex = Math.max(0, lvl.waves.length - 1);
      refreshWaveList();
      render();
    });
  });
}

// ===== 关卡选择器 =====
function refreshLevelSelect(): void {
  const sel = document.getElementById('levelSelect') as HTMLSelectElement;
  sel.innerHTML = '';
  levels.forEach((l, i) => {
    const opt = document.createElement('option');
    opt.value = String(i);
    opt.textContent = `第 ${l.level_id} 关${l.is_boss ? '（Boss）' : ''}`;
    sel.appendChild(opt);
  });
  sel.value = String(currentLevelIndex);
}

function switchLevel(idx: number): void {
  currentLevelIndex = idx;
  activeWaveIndex = 0;
  selection = null;
  refreshLevelSelect();
  refreshWaveList();
  updateLevelProps();
  updateSelProps();
  render();
}

function updateLevelProps(): void {
  const lvl = curLevel();
  (document.getElementById('propLevelId') as HTMLInputElement).value = String(lvl.level_id);
  (document.getElementById('propIsBoss') as HTMLInputElement).checked = lvl.is_boss;
}

// ===== JSON 导入 / 导出 =====
function exportJSON(): void {
  const text = JSON.stringify(levels, null, 2);
  const blob = new Blob([text], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = 'levels.json';
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
  setStatus('已导出 levels.json，请覆盖到 web/levels/ 目录后刷新游戏');
}

function importJSON(text: string): void {
  try {
    const data = JSON.parse(text);
    if (!Array.isArray(data)) throw new Error('根节点必须是数组');
    levels = data.map((l: any) => ({
      level_id: l.level_id ?? 1,
      is_boss: !!l.is_boss,
      player_spawn: { x: l.player_spawn?.x ?? 0.15, y: l.player_spawn?.y ?? 0.5 },
      walls: (l.walls ?? []).map((w: any) => ({
        x: w.x ?? 0, y: w.y ?? 0, w: w.w ?? 0, h: w.h ?? 0,
      })),
      waves: (l.waves ?? []).map((wv: any) => ({
        delay: wv.delay ?? 0,
        spawns: (wv.spawns ?? []).map((s: any) => ({
          preset_id: s.preset_id ?? 0,
          x: s.x ?? 0, y: s.y ?? 0,
        })),
      })),
    }));
    currentLevelIndex = 0;
    activeWaveIndex = 0;
    selection = null;
    refreshLevelSelect();
    refreshWaveList();
    updateLevelProps();
    updateSelProps();
    render();
    setStatus(`导入成功，共 ${levels.length} 关`);
  } catch (e) {
    alert('JSON 解析失败：' + (e as Error).message);
  }
}

// ===== 工具切换 =====
function setTool(t: Tool): void {
  currentTool = t;
  document.querySelectorAll('.tool-btn').forEach(b => {
    b.classList.toggle('active', (b as HTMLElement).dataset.tool === t);
  });
  const hints: Record<Tool, string> = {
    select: '选择模式：点击选中元素，拖动移动；Delete/右键删除',
    wall: '画墙模式：按住鼠标拖出矩形，松开创建',
    enemy: `放敌人模式：当前预设=${PRESET_NAMES[currentPreset]}，点击放置到第 ${activeWaveIndex + 1} 波`,
    spawn: '出生点模式：点击放置玩家出生点（每关唯一）',
    delete: '删除模式：点击元素直接删除',
  };
  setStatus(hints[t]);
}

function setStatus(s: string): void {
  statusBar.textContent = s;
}

// ===== 初始化 =====
async function init(): Promise<void> {
  // 绑定工具按钮
  document.querySelectorAll('.tool-btn').forEach(b => {
    b.addEventListener('click', () => setTool((b as HTMLElement).dataset.tool as Tool));
  });

  (document.getElementById('presetSelect') as HTMLSelectElement).addEventListener('change', (e) => {
    currentPreset = parseInt((e.target as HTMLSelectElement).value, 10);
    if (currentTool === 'enemy') setTool('enemy');
  });

  // 关卡选择
  (document.getElementById('levelSelect') as HTMLSelectElement).addEventListener('change', (e) => {
    switchLevel(parseInt((e.target as HTMLSelectElement).value, 10));
  });

  // 关卡属性
  (document.getElementById('propLevelId') as HTMLInputElement).addEventListener('input', (e) => {
    curLevel().level_id = parseInt((e.target as HTMLInputElement).value, 10) || 1;
    refreshLevelSelect();
  });
  (document.getElementById('propIsBoss') as HTMLInputElement).addEventListener('change', (e) => {
    curLevel().is_boss = (e.target as HTMLInputElement).checked;
    refreshLevelSelect();
  });

  // 波次
  document.getElementById('btnAddWave')!.addEventListener('click', () => {
    curLevel().waves.push({ delay: 1.0, spawns: [] });
    activeWaveIndex = curLevel().waves.length - 1;
    refreshWaveList();
    render();
  });

  // 新建/删除关卡
  document.getElementById('btnAddLevel')!.addEventListener('click', () => {
    const newId = levels.length > 0 ? Math.max(...levels.map(l => l.level_id)) + 1 : 1;
    levels.push({
      level_id: newId,
      is_boss: false,
      player_spawn: { x: 0.15, y: 0.5 },
      walls: [],
      waves: [{ delay: 1.0, spawns: [] }],
    });
    currentLevelIndex = levels.length - 1;
    activeWaveIndex = 0;
    selection = null;
    refreshLevelSelect();
    refreshWaveList();
    updateLevelProps();
    updateSelProps();
    render();
  });
  document.getElementById('btnDelLevel')!.addEventListener('click', () => {
    if (levels.length <= 1) { alert('至少保留一个关卡'); return; }
    if (!confirm(`确定删除第 ${curLevel().level_id} 关？`)) return;
    levels.splice(currentLevelIndex, 1);
    currentLevelIndex = Math.min(currentLevelIndex, levels.length - 1);
    activeWaveIndex = 0;
    selection = null;
    refreshLevelSelect();
    refreshWaveList();
    updateLevelProps();
    updateSelProps();
    render();
  });

  // 导入/导出
  document.getElementById('btnExport')!.addEventListener('click', exportJSON);
  document.getElementById('btnImport')!.addEventListener('click', () => {
    (document.getElementById('fileInput') as HTMLInputElement).click();
  });
  (document.getElementById('fileInput') as HTMLInputElement).addEventListener('change', (e) => {
    const file = (e.target as HTMLInputElement).files?.[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = () => importJSON(reader.result as string);
    reader.readAsText(file);
  });

  // 键盘快捷键
  window.addEventListener('keydown', (e) => {
    // 输入框聚焦时不处理
    const tag = (e.target as HTMLElement).tagName;
    if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') return;
    switch (e.key.toLowerCase()) {
      case 'v': setTool('select'); break;
      case 'w': setTool('wall'); break;
      case 'e': setTool('enemy'); break;
      case 'p': setTool('spawn'); break;
      case 'd': setTool('delete'); break;
      case 'delete':
      case 'backspace':
        if (selection) deleteSelection(selection);
        break;
    }
  });

  // 加载现有 levels.json
  try {
    const resp = await fetch('levels/levels.json');
    if (resp.ok) {
      const text = await resp.text();
      importJSON(text);
      setStatus('已加载 levels.json');
      return;
    }
  } catch { /* 忽略，使用默认关卡 */ }

  // 默认关卡
  levels = [{
    level_id: 1,
    is_boss: false,
    player_spawn: { x: 0.15, y: 0.5 },
    walls: [],
    waves: [{ delay: 1.0, spawns: [] }],
  }];
  currentLevelIndex = 0;
  activeWaveIndex = 0;
  refreshLevelSelect();
  refreshWaveList();
  updateLevelProps();
  updateSelProps();
  render();
  setStatus('未找到 levels.json，已创建空白关卡');
}

init().catch(e => console.error('编辑器初始化失败:', e));
