import {
  RenderCommand, EntityType, ElementType,
  ELEMENT_COLORS, ELEMENT_NAMES, REACTION_NAMES, unpackGauge,
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

  // ===== 敌人（元素光环 × 2 + 元素量条 + 反应标签） =====
  private drawEnemy(ctx: CanvasRenderingContext2D, cmd: RenderCommand): void {
    const { x, y, radius, r, g, b, a,
            element1_type, element2_type,
            element1_gauge, element2_gauge,
            reaction_type } = cmd;
    const key = `enemy_${cmd.x}_${cmd.y}`;

    // 身体：根据肉身 RGB 动态算描边
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

    // ---------- 元素光环：内圈（slot[0]） + 外圈（slot[1]，共存态）----------
    const g1 = unpackGauge(element1_gauge);
    const g2 = unpackGauge(element2_gauge);
    if (element1_type !== 0 && g1 > 0) {
      const color = ELEMENT_COLORS[element1_type] || '#888';
      ctx.beginPath();
      ctx.arc(x, y, radius + 5, 0, Math.PI * 2);
      ctx.strokeStyle = color;
      ctx.lineWidth = 2.5;
      ctx.globalAlpha = 0.55 + Math.min(1, g1) * 0.35;
      ctx.stroke();
      ctx.globalAlpha = 1.0;
    }
    // 第二圈（共存元素，画在更外侧）
    if (element2_type !== 0 && g2 > 0) {
      const color = ELEMENT_COLORS[element2_type] || '#888';
      ctx.beginPath();
      ctx.arc(x, y, radius + 11, 0, Math.PI * 2);
      ctx.strokeStyle = color;
      ctx.lineWidth = 2.0;             // 外圈稍薄
      ctx.globalAlpha = 0.50 + Math.min(1, g2) * 0.30;
      ctx.setLineDash([4, 3]);        // 虚线：和内圈实线形成视觉区分
      ctx.stroke();
      ctx.setLineDash([]);
      ctx.globalAlpha = 1.0;
    }

    // ---------- 元素量条（显示 1~2 段，两段时并排）----------
    const barW = radius * 2.2;
    const barH = 5;
    const barX = x - barW / 2;
    const barY = y + radius + 10;

    ctx.fillStyle = '#222';
    ctx.fillRect(barX, barY, barW, barH);

    const has1 = element1_type !== 0;
    const has2 = element2_type !== 0;
    if (has1 && !has2) {
      // 单元素 → 完整长度
      const color = ELEMENT_COLORS[element1_type] || '#888';
      ctx.fillStyle = color;
      ctx.fillRect(barX, barY, barW * g1, barH);
      ctx.fillStyle = color;
      ctx.font = 'bold 10px monospace';
      ctx.textAlign = 'left';
      ctx.fillText(ELEMENT_NAMES[element1_type] || '', barX, barY - 1);
    } else if (has1 && has2) {
      // 双元素 → 左半显示 slot0，右半显示 slot1
      const color1 = ELEMENT_COLORS[element1_type] || '#888';
      const color2 = ELEMENT_COLORS[element2_type] || '#888';
      ctx.fillStyle = color1;
      ctx.fillRect(barX, barY, (barW / 2) * g1, barH);
      ctx.fillStyle = color2;
      ctx.fillRect(barX + barW / 2, barY, (barW / 2) * g2, barH);
      // 元素名：左名、右名
      ctx.font = 'bold 10px monospace';
      ctx.textAlign = 'left';
      ctx.fillStyle = color1;
      ctx.fillText(ELEMENT_NAMES[element1_type] || '', barX, barY - 1);
      ctx.textAlign = 'right';
      ctx.fillStyle = color2;
      ctx.fillText(ELEMENT_NAMES[element2_type] || '', barX + barW, barY - 1);
    } else if (has2 && !has1) {
      // 理论上不会出现（写 slot 时先写 0 再写 1），兜底
      const color = ELEMENT_COLORS[element2_type] || '#888';
      ctx.fillStyle = color;
      ctx.fillRect(barX, barY, barW * g2, barH);
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
    ctx.fillText('数字键1-7: 切换玩家元素', 10, 40);

    // ===== 左侧：数字键 ↔ 元素映射面板 =====
    const panelX = 14;                // 面板左上 X
    const panelY = 70;                // 面板左上 Y
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
      const rowW = 142;
      const name = ELEMENT_NAMES[elem] || '';
      const color = ELEMENT_COLORS[elem] || '#888';

      if (elem === this.currentElem) {
        ctx.fillStyle = 'rgba(255,255,255,0.08)';
        ctx.fillRect(panelX, y - 18, rowW, rowH - 2);
        ctx.strokeStyle = '#ffffff';
        ctx.lineWidth = 2;
        ctx.strokeRect(panelX, y - 18, rowW, rowH - 2);
      }

      ctx.fillStyle = '#ffffff';
      ctx.textAlign = 'left';
      ctx.fillText(key, panelX + 8, y);

      ctx.fillStyle = 'rgba(255,255,255,0.4)';
      ctx.fillText('-', panelX + 28, y);

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