import {
  RenderCommand, EntityType, ElementType,
  ELEMENT_COLORS, ELEMENT_NAMES, REACTION_NAMES,
} from './types.js';

export class Renderer {
  private ctx: CanvasRenderingContext2D;
  private width: number;
  private height: number;

  // 每个敌人上次触发的反应 + 标签渐隐计时器
  private enemyReactions: Map<string, { type: number; t: number }> = new Map();

  // UI：左侧元素面板高亮的当前选中元素（默认 Pyro，和初始 spawn_player 一致）
  private currentElem: ElementType = ElementType.Pyro;

  // 设置当前选中的元素（按键按下时由 main.ts 调用，立即更新白框高亮）
  setCurrentElement(elem: ElementType): void {
    this.currentElem = elem;
  }

  constructor(canvas: HTMLCanvasElement) {
    this.ctx = canvas.getContext('2d')!;
    this.width = canvas.width;
    this.height = canvas.height;
  }

  draw(commands: RenderCommand[]): void {
    const ctx = this.ctx;

    // 背景
    ctx.fillStyle = '#1a1a2e';
    ctx.fillRect(0, 0, this.width, this.height);

    // 网格
    ctx.strokeStyle = 'rgba(255,255,255,0.04)';
    ctx.lineWidth = 1;
    for (let x = 0; x < this.width; x += 40) {
      ctx.beginPath();
      ctx.moveTo(x, 0); ctx.lineTo(x, this.height);
      ctx.stroke();
    }
    for (let y = 0; y < this.height; y += 40) {
      ctx.beginPath();
      ctx.moveTo(0, y); ctx.lineTo(this.width, y);
      ctx.stroke();
    }

    // 子弹（底层）+ 敌人 + 玩家（顶层）分三层画，防止遮挡
    const bullets: RenderCommand[] = [];
    const enemies: RenderCommand[] = [];
    const player: RenderCommand[] = [];
    for (const c of commands) {
      if (c.entity_type === EntityType.Bullet) bullets.push(c);
      else if (c.entity_type === EntityType.Enemy)  enemies.push(c);
      else player.push(c);
    }
    for (const c of bullets)   this.drawBullet(ctx, c);
    for (const c of enemies)     this.drawEnemy(ctx, c);
    for (const c of player)      this.drawPlayer(ctx, c);

    this.drawUI(ctx);
  }

  // ===== 玩家（含朝向箭头） =====
  private drawPlayer(ctx: CanvasRenderingContext2D, cmd: RenderCommand): void {
    const { x, y, radius, r, g, b, a, facing_dx, facing_dy } = cmd;

    // 身体
    ctx.beginPath();
    ctx.arc(x, y, radius, 0, Math.PI * 2);
    ctx.fillStyle = `rgba(${r},${g},${b},${a / 255})`;
    ctx.fill();
    ctx.strokeStyle = '#ffffff';
    ctx.lineWidth = 2;
    ctx.stroke();

    // 朝向小三角（显示面朝方向）
    if ((facing_dx !== 0 || facing_dy !== 0)) {
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
        y + facing_dy * (radius + 4) + perpY * w,
      );
      ctx.lineTo(
        x + facing_dx * (radius + 4) - perpX * w,
        y + facing_dy * (radius + 4) - perpY * w,
      );
      ctx.closePath();
      ctx.fillStyle = '#ffffff';
      ctx.fill();
    }
  }

  // ===== 敌人（元素光环 + 元素量条 + 反应标签） =====
  private drawEnemy(ctx: CanvasRenderingContext2D, cmd: RenderCommand): void {
    const { x, y, radius, r, g, b, a, element_type, element_gauge, reaction_type } = cmd;
    const key = `enemy_${cmd.x}_${cmd.y}`;

    // 身体：根据肉身 RGB 动态算描边（原 RGB 各通道 ×0.6 → 自身的深色描边，不会再写死火红边）
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

    // 元素光环
    if (element_type !== 0 && element_gauge > 0) {
      const color = ELEMENT_COLORS[element_type] || '#888';
      ctx.beginPath();
      ctx.arc(x, y, radius + 5, 0, Math.PI * 2);
      ctx.strokeStyle = color;
      ctx.lineWidth = 2.5;
      ctx.globalAlpha = 0.55 + Math.min(1, element_gauge) * 0.35;
      ctx.stroke();
      ctx.globalAlpha = 1.0;
    }

    // 元素量条（随时间衰减的视觉效果）
    const barW = radius * 2.2;
    const barH = 5;
    const barX = x - barW / 2;
    const barY = y + radius + 10;

    ctx.fillStyle = '#222';
    ctx.fillRect(barX, barY, barW, barH);

    if (element_type !== 0) {
      const color = ELEMENT_COLORS[element_type] || '#888';
      const g = Math.max(0, Math.min(1, element_gauge));
      ctx.fillStyle = color;
      ctx.fillRect(barX, barY, barW * g, barH);

      // 元素名
      ctx.fillStyle = color;
      ctx.font = 'bold 10px monospace';
      ctx.textAlign = 'left';
      ctx.fillText(ELEMENT_NAMES[element_type] || '', barX, barY - 1);
    }

    // 反应文字标签（渐隐）
    const cached = this.enemyReactions.get(key) || { type: 0, t: 0 };
    if (reaction_type !== 0) {
      cached.type = reaction_type;
      cached.t = 1.5;
    }
    if (cached.t > 0) {
      cached.t -= 1 / 60;
      const name = REACTION_NAMES[cached.type] || '';
      if (name) {
        ctx.font = 'bold 14px monospace';
        ctx.textAlign = 'center';
        ctx.fillStyle = '#ffdd33';
        ctx.globalAlpha = Math.min(1, cached.t);
        ctx.fillText(name, x, y - radius - 18 - (1.5 - cached.t) * 20);
        ctx.globalAlpha = 1.0;
      }
    }
    this.enemyReactions.set(key, cached);
  }

  // ===== 子弹 =====
  private drawBullet(ctx: CanvasRenderingContext2D, cmd: RenderCommand): void {
    const { x, y, radius, r, g, b, a } = cmd;

    // 尾焰 + 核心
    const grad = ctx.createRadialGradient(x, y, 0, x, y, radius + 6);
    grad.addColorStop(0, `rgba(${r},${g},${b},220)`);
    grad.addColorStop(0.6, `rgba(${r},${g},${b},120)`);
    grad.addColorStop(1, 'rgba(100,180,255,0)');
    ctx.fillStyle = grad;
    ctx.beginPath();
    ctx.arc(x, y, radius + 6, 0, Math.PI * 2);
    ctx.fill();

    ctx.beginPath();
    ctx.arc(x, y, radius, 0, Math.PI * 2);
    ctx.fillStyle = `rgba(${r},${g},${b},${a / 255})`;
    ctx.fill();
  }

  private drawUI(ctx: CanvasRenderingContext2D): void {
    // ===== 顶部操作说明 =====
    ctx.fillStyle = 'rgba(255,255,255,0.75)';
    ctx.font = '13px monospace';
    ctx.textAlign = 'left';
    const curElemName = ELEMENT_NAMES[this.currentElem] || '';
    ctx.fillText(`WASD: 移动   鼠标移动: 改变朝向   鼠标左键: 发射${curElemName}元素子弹`, 10, 22);
    ctx.fillText('反应: 水+火 → 蒸发(×1.5)   火+水 → 蒸发(×2.0)   数字键1-7: 切换玩家元素', 10, 40);

    // ===== 左侧：数字键 ↔ 元素映射面板 =====
    const panelX = 14;                // 面板左上 X
    const panelY = 70;                // 面板左上 Y（避开顶部两行操作说明
    const rowH = 28;                  // 每行高度
    const entries: Array<{ key: string; elem: ElementType }> = [
      { key: '1', elem: ElementType.Pyro    },
      { key: '2', elem: ElementType.Hydro   },
      { key: '3', elem: ElementType.Electro },
      { key: '4', elem: ElementType.Cryo    },
      { key: '5', elem: ElementType.Anemo   },
      { key: '6', elem: ElementType.Geo     },
      { key: '7', elem: ElementType.Dendro  },
    ];

    ctx.font = '14px monospace';
    for (let i = 0; i < entries.length; i++) {
      const { key, elem } = entries[i];
      const y = panelY + i * rowH;
      const rowW = 142;               // 白框宽度（包下整行）
      const name = ELEMENT_NAMES[elem] || '';
      const color = ELEMENT_COLORS[elem] || '#888';

      // 当前元素：白框 + 半透明背景框住整行
      if (elem === this.currentElem) {
        ctx.fillStyle = 'rgba(255,255,255,0.08)';
        ctx.fillRect(panelX, y - 18, rowW, rowH - 2);
        ctx.strokeStyle = '#ffffff';
        ctx.lineWidth = 2;
        ctx.strokeRect(panelX, y - 18, rowW, rowH - 2);
      }

      // 数字键（左对齐）
      ctx.fillStyle = '#ffffff';
      ctx.textAlign = 'left';
      ctx.fillText(key, panelX + 8, y);

      // 连接符 -
      ctx.fillStyle = 'rgba(255,255,255,0.4)';
      ctx.fillText('-', panelX + 28, y);

      // 元素色方块 + 元素名（按元素色显示）
      const sqX = panelX + 44;
      const sqY = y - 12;
      ctx.fillStyle = color;
      ctx.fillRect(sqX, sqY, 14, 14);
      ctx.strokeStyle = 'rgba(255,255,255,0.5)';
      ctx.lineWidth = 1;
      ctx.strokeRect(sqX, sqY, 14, 14);

      ctx.fillStyle = color;
      ctx.fillText(name, panelX + 68, y);
    }
  }
}