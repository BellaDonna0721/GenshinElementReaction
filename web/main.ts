import {
  InputState, ElementType, HealthBarInfo, DamageNumberInfo, WallRenderInfo,
} from './types.js';
import { Renderer } from './render.js';

interface GameModule {
  _wasm_init: () => void;
  _wasm_update: (dt: number) => void;
  _wasm_set_input: (
    up: number, down: number, left: number, right: number,
    mouse_x: number, mouse_y: number, mouse_click: number,
    mouse_right_click: number, element_key: number
  ) => void;
  _wasm_get_render_data: () => number;
  _wasm_get_render_count: () => number;
  _wasm_get_health_data: () => number;
  _wasm_get_health_count: () => number;
  _wasm_get_damage_numbers: () => number;
  _wasm_get_damage_number_count: () => number;
  _wasm_get_wall_data: () => number;
  _wasm_get_wall_count: () => number;
  _wasm_spawn_enemy: (preset_id: number, x: number, y: number) => number;
  _wasm_remove_enemy: (entity_id: number) => void;
  _wasm_clear_enemies: () => void;
  _wasm_get_alive_enemy_count: () => number;
  _wasm_reset: () => void;
  _wasm_start_game: () => void;
  _wasm_get_game_phase: () => number;
  _wasm_get_score: () => number;
  _wasm_get_level: () => number;
  _wasm_get_player_hp_ratio: () => number;
  _wasm_get_buff_choices: (out_ptr: number) => number;
  _wasm_select_buff: (buff_id: number) => void;
  _wasm_get_active_buff_count: () => number;
  _wasm_get_active_buff: (idx: number) => number;
  _malloc: (size: number) => number;
  _free: (ptr: number) => void;
  HEAPF32: Float32Array;
  HEAPU8: Uint8Array;
  HEAP32: Int32Array;
}

// 增益信息表（与 C++ BuffRegistry 对齐）
const BUFF_INFO: Array<{ name: string; desc: string }> = [
  { name: '攻击强化', desc: '攻击力 +20%' },
  { name: '疾速射击', desc: '攻击速度 +15%' },
  { name: '疾风步',   desc: '移动速度 +15%' },
  { name: '强健体魄', desc: '生命上限 +30 并回满' },
  { name: '治愈之光', desc: '立即回复 50% 生命值' },
  { name: '元素精通', desc: '元素反应伤害 +25%' },
  { name: '穿透弹',   desc: '子弹伤害 +5' },
  { name: '元素护盾', desc: '获得一层护盾' },
];

let wasmModule: GameModule;
let renderer: Renderer;
let canvas: HTMLCanvasElement;
let lastTime: number = 0;
let running: boolean = false;

// 场景状态：menu 主菜单 / help 帮助 / training 训练模式 / playing 关卡模式
type Scene = 'menu' | 'help' | 'training' | 'playing';
let currentScene: Scene = 'menu';
let wasmInited: boolean = false;  // wasm 是否已初始化

const input: InputState = {
  moveUp: false,
  moveDown: false,
  moveLeft: false,
  moveRight: false,
  mouseX: 0,
  mouseY: 0,
  mouseClick: false,
  mouseRightClick: false,
  elemKey: 0,
};

function onKeyDown(e: KeyboardEvent): void {
  switch (e.key.toLowerCase()) {
    case 'w': case 'arrowup':    input.moveUp = true; break;
    case 's': case 'arrowdown':  input.moveDown = true; break;
    case 'a': case 'arrowleft':  input.moveLeft = true; break;
    case 'd': case 'arrowright': input.moveRight = true; break;
    case '1': input.elemKey = 1; renderer.setCurrentElement(ElementType.Pyro);    break; // 火
    case '2': input.elemKey = 2; renderer.setCurrentElement(ElementType.Hydro);   break; // 水
    case '3': input.elemKey = 3; renderer.setCurrentElement(ElementType.Electro); break; // 雷
    case '4': input.elemKey = 4; renderer.setCurrentElement(ElementType.Cryo);    break; // 冰
    case '5': input.elemKey = 5; renderer.setCurrentElement(ElementType.Anemo);   break; // 风
    case '6': input.elemKey = 6; renderer.setCurrentElement(ElementType.Geo);     break; // 岩
    case '7': input.elemKey = 7; renderer.setCurrentElement(ElementType.Dendro);  break; // 草
  }
}

function onKeyUp(e: KeyboardEvent): void {
  switch (e.key.toLowerCase()) {
    case 'w': case 'arrowup':    input.moveUp = false; break;
    case 's': case 'arrowdown':  input.moveDown = false; break;
    case 'a': case 'arrowleft':  input.moveLeft = false; break;
    case 'd': case 'arrowright': input.moveRight = false; break;
  }
}

function onMouseMove(e: MouseEvent): void {
  const rect = canvas.getBoundingClientRect();
  const scaleX = canvas.width / rect.width;
  const scaleY = canvas.height / rect.height;
  input.mouseX = (e.clientX - rect.left) * scaleX;
  input.mouseY = (e.clientY - rect.top) * scaleY;
}

function onMouseDown(e: MouseEvent): void {
  if (e.button === 0) input.mouseClick = true;
  if (e.button === 2) input.mouseRightClick = true;
}
function onMouseUp(e: MouseEvent): void {
  if (e.button === 0) input.mouseClick = false;
  if (e.button === 2) input.mouseRightClick = false;
}

function syncInput(): void {
  wasmModule._wasm_set_input(
    input.moveUp ? 1 : 0,
    input.moveDown ? 1 : 0,
    input.moveLeft ? 1 : 0,
    input.moveRight ? 1 : 0,
    input.mouseX,
    input.mouseY,
    input.mouseClick ? 1 : 0,
    input.mouseRightClick ? 1 : 0,
    input.elemKey,
  );
  // element_key 只触发一次（脉冲式）：处理完这帧就归零，避免每秒切 60 次
  input.elemKey = 0;
}

// 读取 RenderCommand（严格 32 字节 stride）
// C++ ↔ TS 统一协议（必须与 OutputSystem.h 的 struct RenderCommand 逐字节对齐）:
//  [0,8)    x, y                        (f32 × 2)
//  [8,12)   radius                      (f32)
//  [12,16)  r, g, b, a                  (u8 × 4)
//  [16]     entity_type                 (u8)
//  [17]     element1_type (内圈附着元素) (u8, ElementType enum, 0=None)
//  [18]     reaction_mask_lo            (u8, 位掩码低 8 位，bit i → ReactionType#i)
//  [19]     element2_type (外圈共存元素) (u8, ElementType enum, 0=None)
//  [20]     element1_gauge              (u8, 0-255 线性 → 进度 0-100%)
//  [21]     element2_gauge              (u8, 0-255 线性 → 进度 0-100%)
//  [22]     bullet_strength             (u8, ElementStrength: 0Weak 1Medium 2Strong 3Extra; 敌人玩家填0)
//  [23]     reaction_mask_hi            (u8, 位掩码高 8 位，与 [18] 合并)
//  [24,28)  facing_dx                   (f32)
//  [28,32)  facing_dy                   (f32)
const STRIDE = 32;
function readRenderCommands(): import('./types.js').RenderCommand[] {
  const count = wasmModule._wasm_get_render_count();
  if (count <= 0) return [];

  const ptr = wasmModule._wasm_get_render_data();
  const heap = wasmModule.HEAPU8;
  const f32  = new Float32Array(heap.buffer);
  const result: import('./types.js').RenderCommand[] = [];

  for (let i = 0; i < count; i++) {
    const base = ptr + i * STRIDE;
    result.push({
      x:              f32[ base          / 4],
      y:              f32[(base + 4)    / 4],
      radius:         f32[(base + 8)    / 4],
      r:              heap[base + 12],
      g:              heap[base + 13],
      b:              heap[base + 14],
      a:              heap[base + 15],
      entity_type:    heap[base + 16],
      element1_type:  heap[base + 17],
      reaction_mask_lo: heap[base + 18],
      element2_type:  heap[base + 19],
      element1_gauge: heap[base + 20],
      element2_gauge: heap[base + 21],
      bullet_strength:heap[base + 22],
      reaction_mask_hi: heap[base + 23],
      facing_dx:      f32[(base + 24)   / 4],
      facing_dy:      f32[(base + 28)   / 4],
    });
  }
  return result;
}

// 读取血条数据（HealthBarInfo，16 字节 stride）
//   [0,4)  x       (f32)
//   [4,8)  y       (f32)
//   [8,12) width   (f32)
//   [12]   ratio   (u8, 0-255 → 0-100%)
const HEALTH_STRIDE = 16;
function readHealthBars(): HealthBarInfo[] {
  const count = wasmModule._wasm_get_health_count();
  if (count <= 0) return [];

  const ptr = wasmModule._wasm_get_health_data();
  const heap = wasmModule.HEAPU8;
  const f32  = new Float32Array(heap.buffer);
  const result: HealthBarInfo[] = [];

  for (let i = 0; i < count; i++) {
    const base = ptr + i * HEALTH_STRIDE;
    result.push({
      x:     f32[ base        / 4],
      y:     f32[(base + 4)  / 4],
      width: f32[(base + 8)  / 4],
      ratio: heap[base + 12],
      enemy_type: heap[base + 13],
    });
  }
  return result;
}

// 读取伤害飘字数据（DamageNumberInfo，16 字节 stride）
//   [0,4)  x       (f32)
//   [4,8)  y       (f32)
//   [8,12) amount  (i32)
//   [12]   r       (u8)
//   [13]   g       (u8)
//   [14]   b       (u8)
const DAMAGE_STRIDE = 16;
function readDamageNumbers(): DamageNumberInfo[] {
  const count = wasmModule._wasm_get_damage_number_count();
  if (count <= 0) return [];

  const ptr = wasmModule._wasm_get_damage_numbers();
  const heap = wasmModule.HEAPU8;
  const f32  = new Float32Array(heap.buffer);
  const i32  = new Int32Array(heap.buffer);
  const result: DamageNumberInfo[] = [];

  for (let i = 0; i < count; i++) {
    const base = ptr + i * DAMAGE_STRIDE;
    result.push({
      x:      f32[ base        / 4],
      y:      f32[(base + 4)  / 4],
      amount: i32[(base + 8)  / 4],
      r:      heap[base + 12],
      g:      heap[base + 13],
      b:      heap[base + 14],
      is_transformative: heap[base + 15],
    });
  }
  return result;
}

// 读取墙体数据（WallRenderInfo，20 字节 stride）
//   [0,4)   x       (f32)
//   [4,8)   y       (f32)
//   [8,12)  width   (f32)
//   [12,16) height  (f32)
//   [16]    r       (u8)
//   [17]    g       (u8)
//   [18]    b       (u8)
//   [19]    a       (u8)
const WALL_STRIDE = 20;
function readWalls(): WallRenderInfo[] {
  const count = wasmModule._wasm_get_wall_count();
  if (count <= 0) return [];

  const ptr = wasmModule._wasm_get_wall_data();
  const heap = wasmModule.HEAPU8;
  const f32  = new Float32Array(heap.buffer);
  const result: WallRenderInfo[] = [];

  for (let i = 0; i < count; i++) {
    const base = ptr + i * WALL_STRIDE;
    result.push({
      x:      f32[ base        / 4],
      y:      f32[(base + 4)  / 4],
      width:  f32[(base + 8)  / 4],
      height: f32[(base + 12) / 4],
      r:      heap[base + 16],
      g:      heap[base + 17],
      b:      heap[base + 18],
      a:      heap[base + 19],
    });
  }
  return result;
}

function gameLoop(timestamp: number): void {
  if (!running) return;
  if (currentScene !== 'training' && currentScene !== 'playing') return;

  const dt = Math.min((timestamp - lastTime) / 1000, 0.1);
  lastTime = timestamp;

  syncInput();
  wasmModule._wasm_update(dt);

  // 获取玩家血量比例（0~100）
  const playerHpRatio = wasmModule._wasm_get_player_hp_ratio() / 100;

  renderer.draw(readRenderCommands(), readHealthBars(), readDamageNumbers(), readWalls(), playerHpRatio);

  // 关卡模式：检查游戏阶段
  if (currentScene === 'playing') {
    const phase = wasmModule._wasm_get_game_phase();
    if (phase === 1) {
      // LevelComplete → 弹增益选择
      running = false;
      showBuffSelect();
      return;
    } else if (phase === 2) {
      // GameOver → 弹结算
      running = false;
      showGameOver();
      return;
    }
  }

  requestAnimationFrame(gameLoop);
}

// ===== 场景切换 =====
function showScreen(scene: Scene): void {
  currentScene = scene;
  document.querySelectorAll('.screen').forEach(el => el.classList.remove('active'));
  if (scene === 'menu')      document.getElementById('menuScreen')?.classList.add('active');
  if (scene === 'help')      document.getElementById('helpScreen')?.classList.add('active');
  if (scene === 'training' || scene === 'playing') {
    document.getElementById('gameScreen')?.classList.add('active');
  }
}

// 进入训练模式：懒初始化 wasm，启动游戏循环
async function enterTraining(): Promise<void> {
  showScreen('training');

  if (!wasmInited) {
    // @ts-ignore
    wasmModule = await (window as any).GameModule();
    wasmModule._wasm_init();
    renderer.setCurrentElement(ElementType.Pyro);
    wasmInited = true;
  } else {
    wasmModule._wasm_reset();
    renderer.setCurrentElement(ElementType.Pyro);
  }

  input.moveUp = input.moveDown = input.moveLeft = input.moveRight = false;
  input.mouseClick = input.mouseRightClick = false;

  running = true;
  lastTime = performance.now();
  requestAnimationFrame(gameLoop);
}

// 开始游戏：关卡模式
async function enterGame(): Promise<void> {
  showScreen('playing');

  if (!wasmInited) {
    // @ts-ignore
    wasmModule = await (window as any).GameModule();
    wasmModule._wasm_init();
    wasmInited = true;
  }
  // 重置到第 1 关
  wasmModule._wasm_start_game();
  renderer.setCurrentElement(ElementType.Pyro);

  input.moveUp = input.moveDown = input.moveLeft = input.moveRight = false;
  input.mouseClick = input.mouseRightClick = false;

  running = true;
  lastTime = performance.now();
  requestAnimationFrame(gameLoop);
}

// 退出到主菜单
function exitToMenu(): void {
  running = false;
  showScreen('menu');
}

// ===== 增益选择弹层 =====
function showBuffSelect(): void {
  // 从 wasm 读取 3 个候选增益
  const ptr = wasmModule._malloc(12);  // 3 个 int
  const n = wasmModule._wasm_get_buff_choices(ptr);
  const choices: number[] = [];
  for (let i = 0; i < n; i++) {
    choices.push(wasmModule.HEAP32[(ptr >> 2) + i]);
  }
  wasmModule._free(ptr);

  const overlay = document.getElementById('overlay')!;
  const panel = document.getElementById('buffPanel')!;
  const cards = document.getElementById('buffCards')!;
  cards.innerHTML = '';

  choices.forEach(id => {
    const info = BUFF_INFO[id] || { name: '???', desc: '' };
    const card = document.createElement('div');
    card.className = 'buff-card';
    card.innerHTML = `<div class="buff-name">${info.name}</div><div class="buff-desc">${info.desc}</div>`;
    card.addEventListener('click', () => {
      wasmModule._wasm_select_buff(id);
      hideOverlay();
      // 继续游戏循环
      running = true;
      lastTime = performance.now();
      requestAnimationFrame(gameLoop);
    });
    cards.appendChild(card);
  });

  overlay.classList.add('active');
  panel.classList.add('active');
}

// ===== 游戏结算弹层 =====
function showGameOver(): void {
  const score = wasmModule._wasm_get_score();
  const level = wasmModule._wasm_get_level();

  // 读取已选增益
  const buffCount = wasmModule._wasm_get_active_buff_count();
  const buffNames: string[] = [];
  for (let i = 0; i < buffCount; i++) {
    const id = wasmModule._wasm_get_active_buff(i);
    if (id >= 0 && BUFF_INFO[id]) buffNames.push(BUFF_INFO[id].name);
  }

  const overlay = document.getElementById('overlay')!;
  const panel = document.getElementById('gameOverPanel')!;
  document.getElementById('goScore')!.textContent = String(score);
  document.getElementById('goLevel')!.textContent = String(level);
  const buffList = document.getElementById('goBuffs')!;
  buffList.innerHTML = buffNames.length > 0
    ? buffNames.map(n => `<span class="buff-tag">${n}</span>`).join('')
    : '<span style="color:#666;">无</span>';

  overlay.classList.add('active');
  panel.classList.add('active');
}

function hideOverlay(): void {
  document.getElementById('overlay')?.classList.remove('active');
  document.querySelectorAll('.overlay-panel').forEach(p => p.classList.remove('active'));
}

async function init(): Promise<void> {
  canvas = document.getElementById('gameCanvas') as HTMLCanvasElement;
  if (!canvas) throw new Error('Canvas not found');
  canvas.width = 1000;
  canvas.height = 700;
  renderer = new Renderer(canvas);

  window.addEventListener('keydown', onKeyDown);
  window.addEventListener('keyup',   onKeyUp);
  canvas.addEventListener('mousemove', onMouseMove);
  canvas.addEventListener('mousedown', onMouseDown);
  window.addEventListener('mouseup',   onMouseUp);
  canvas.addEventListener('contextmenu', (e) => e.preventDefault());

  // ===== 主菜单按钮 =====
  document.getElementById('btnStart')?.addEventListener('click', () => {
    enterGame().catch(err => console.error('开始游戏失败:', err));
  });
  document.getElementById('btnTraining')?.addEventListener('click', () => {
    enterTraining().catch(err => console.error('进入训练模式失败:', err));
  });
  document.getElementById('btnHelp')?.addEventListener('click', () => {
    showScreen('help');
  });

  // ===== 帮助界面返回 =====
  document.getElementById('btnHelpBack')?.addEventListener('click', () => {
    showScreen('menu');
  });

  // ===== 游戏界面按钮 =====
  document.getElementById('btnReset')?.addEventListener('click', () => {
    if (!wasmInited) return;
    if (currentScene === 'playing') {
      wasmModule._wasm_start_game();  // 关卡模式：重开
    } else {
      wasmModule._wasm_reset();       // 训练模式：重置
    }
    renderer.setCurrentElement(ElementType.Pyro);
  });
  document.getElementById('btnBackMenu')?.addEventListener('click', () => {
    exitToMenu();
  });

  // ===== 结算界面返回主菜单 =====
  document.getElementById('btnGameOverMenu')?.addEventListener('click', () => {
    hideOverlay();
    exitToMenu();
  });

  // 默认停留在主菜单
  showScreen('menu');
  console.log('[GenshinElement] 初始化完成');
}

init().catch(err => console.error('Init failed:', err));