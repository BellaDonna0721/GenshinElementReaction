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
export interface RenderCommand {
  x: number;
  y: number;
  radius: number;
  r: number;
  g: number;
  b: number;
  a: number;
  entity_type: number;
  element_type: number;
  reaction_type: number;
  element_gauge: number;
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
  mouseClick: boolean;
  elemKey: number;  //数字键 1-7 切玩家元素（0=没按，1=火…7=草）
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
  13: '激化',
};