export enum ElementType {
  None = 0,
  Pyro,
  Hydro,
  Electro,
  Cryo,
  Anemo,
  Geo,
  Dendro,
}

export enum EntityType {
  Player = 0,
  Enemy = 1,
  Bullet = 2,
}

// 与 C++ 侧 RenderCommand 严格对齐（32 字节）
// byte layout:
//   [0,8)    x, y                 (f32, f32)
//   [8,12)   radius               (f32)
//   [12,16)  r, g, b, a           (u8x4)
//   [16]     entity_type          (u8)
//   [17]     element1_type (内圈) (u8)
//   [18]     reaction_type        (u8)
//   [19]     element2_type (外圈) (u8)
//   [20]     element1_gauge (0-255 → 0-4.0f) (u8)
//   [21]     element2_gauge (0-255 → 0-4.0f) (u8)
//   [22,24)  _pad2, _pad3         (u8x2)
//   [24,28)  facing_dx            (f32)
//   [28,32)  facing_dy            (f32)
export interface RenderCommand {
  x: number;
  y: number;
  radius: number;
  r: number;
  g: number;
  b: number;
  a: number;
  entity_type: number;
  element1_type: number;   // 内圈元素（原来的单环）
  reaction_type: number;   // 脉冲反应名
  element2_type: number;   // 外圈元素（共存第二元素）
  element1_gauge: number;  // 内圈归一化 0-255
  element2_gauge: number;  // 外圈归一化 0-255
  facing_dx: number;
  facing_dy: number;
}

export interface InputState {
  moveUp: boolean;
  moveDown: boolean;
  moveLeft: boolean;
  moveRight: boolean;
  mouseX: number;
  mouseY: number;
  mouseClick: boolean;       // 左键 → Weak 子弹
  mouseRightClick: boolean;  // 右键 → Strong 子弹
  elemKey: number;           // 数字键 1-7 切玩家元素（0=没按，1=火…7=草）
}

export const ELEMENT_COLORS: Record<number, string> = {
  [ElementType.None]:    '#888888',
  [ElementType.Pyro]:    '#EF7938',
  [ElementType.Hydro]:   '#4CC2F1',
  [ElementType.Electro]: '#AF8CF0',
  [ElementType.Cryo]:    '#9FD6E0',
  [ElementType.Anemo]:   '#75D2B2',
  [ElementType.Geo]:     '#FAB632',
  [ElementType.Dendro]:  '#A5C83B',
};

export const ELEMENT_NAMES: Record<number, string> = {
  [ElementType.None]:    '',
  [ElementType.Pyro]:    '火',
  [ElementType.Hydro]:   '水',
  [ElementType.Electro]: '雷',
  [ElementType.Cryo]:    '冰',
  [ElementType.Anemo]:   '风',
  [ElementType.Geo]:     '岩',
  [ElementType.Dendro]:  '草',
};

export const REACTION_NAMES: Record<number, string> = {
  0: '',
  1: '蒸发',
  2: '蒸发',
  3: '融化',
  4: '融化',
  5: '超载',
  6: '感电',
  7: '冻结',
  8: '超导',
  9: '扩散',
  10: '结晶',
  11: '燃烧',
  12: '绽放',
};

// C++ 侧 gauge 打包约定：0-255 u8 线性映射 0-4.0f actual_gauge
// TS 侧反解：只要比例即可，不一定要绝对值
export function unpackGauge(u8: number): number {
  return Math.max(0, Math.min(1, u8 / 255));
}
