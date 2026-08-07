import {
  InputState, ElementType,
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
  _wasm_reset: () => void;
  HEAPF32: Float32Array;
  HEAPU8: Uint8Array;
}

let wasmModule: GameModule;
let renderer: Renderer;
let canvas: HTMLCanvasElement;
let lastTime: number = 0;
let running: boolean = false;

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
//  [18]     reaction_type               (u8, ReactionType enum, 脉冲一次性)
//  [19]     element2_type (外圈共存元素) (u8, ElementType enum, 0=None)
//  [20]     element1_gauge              (u8, 0-255 线性 → 0-4.0f actual_gauge)
//  [21]     element2_gauge              (u8, 0-255 线性 → 0-4.0f actual_gauge)
//  [22,24)  _pad2, _pad3                (u8 × 2, 对齐填充)
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
      reaction_type:  heap[base + 18],
      element2_type:  heap[base + 19],
      element1_gauge: heap[base + 20],
      element2_gauge: heap[base + 21],
      facing_dx:      f32[(base + 24)   / 4],
      facing_dy:      f32[(base + 28)   / 4],
    });
  }
  return result;
}

function gameLoop(timestamp: number): void {
  if (!running) return;
  const dt = Math.min((timestamp - lastTime) / 1000, 0.1);
  lastTime = timestamp;

  syncInput();
  wasmModule._wasm_update(dt);
  renderer.draw(readRenderCommands());

  requestAnimationFrame(gameLoop);
}

async function init(): Promise<void> {
  canvas = document.getElementById('gameCanvas') as HTMLCanvasElement;
  if (!canvas) throw new Error('Canvas not found');
  canvas.width = 800;
  canvas.height = 600;
  renderer = new Renderer(canvas);

  // @ts-ignore
  wasmModule = await (window as any).GameModule();
  wasmModule._wasm_init();
  renderer.setCurrentElement(ElementType.Pyro); // 初始玩家是 Pyro，面板白框默认高亮火

  window.addEventListener('keydown', onKeyDown);
  window.addEventListener('keyup',   onKeyUp);
  canvas.addEventListener('mousemove', onMouseMove);
  canvas.addEventListener('mousedown', onMouseDown);
  window.addEventListener('mouseup',   onMouseUp);
  canvas.addEventListener('contextmenu', (e) => e.preventDefault());

  document.getElementById('btnReset')?.addEventListener('click', () => {
    wasmModule._wasm_reset();
  });

  running = true;
  lastTime = performance.now();
  requestAnimationFrame(gameLoop);
  console.log('[GenshinElement] 初始化完成');
  console.log('操作: WASD 移动 | 鼠标移动改变朝向 | 鼠标左键发射子弹 | 数字键1-7切换玩家元素(1火2水3雷4冰5风6岩7草)');
}

init().catch(err => console.error('Init failed:', err));