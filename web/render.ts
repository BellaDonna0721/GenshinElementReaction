import {
  RenderCommand, EntityType, ElementType,
  ELEMENT_COLORS, ELEMENT_NAMES, REACTION_NAMES, unpackGauge,
} from './types.js';

export class Renderer {
  private ctx: CanvasRenderingContext2D;
  private width: number;
  private height: number;

  // 每个敌人本帧仍在飘字的反应列表（支持一帧多个反应堆叠显示）
  // 每个元素 { type, t }：type 为 ReactionType 枚举，t 为剩余显示时长 s
  private enemyReactions: Map<string, { items: Array<{ type: number; t: number }> }> = new Map();

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
            reaction_mask_lo, reaction_mask_hi } = cmd;
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

    // 反应文字标签（渐隐 + 堆叠，一帧多反应垂直排列）
    const cached = this.enemyReactions.get(key) || { items: [] };

    // 1) 把本帧位掩码里标记过的新反应，全部 push 进 cache（每项独立渐隐）
    //    位索引 i = ReactionType；bit=1 表示本帧该反应实际发生过
    const mask: number = (reaction_mask_lo & 0xFF) | ((reaction_mask_hi & 0xFF) << 8);
    for (let i = 1; i <= 16; i++) {
      if ((mask & (1 << i)) !== 0) {
        cached.items.push({ type: i, t: 1.5 });
      }
    }

    // 2) 每帧扣减剩余时长，过滤掉已过期的；再按"剩余时间从大到小"保持堆叠顺序稳定
    const dtFrame = 1 / 60;
    cached.items = cached.items
      .map(it => ({ ...it, t: it.t - dtFrame }))
      .filter(it => it.t > 0)
      .sort((a, b) => b.t - a.t);

    // 3) 依次画出每一条：新反应（剩余时间大）在下，老的在上（越飞越上）
    //    效果：本帧同时触发的两条，从同一高度起步向上飘；旧的在上方继续渐隐
    if (cached.items.length > 0) {
      ctx.font = 'bold 14px monospace';
      ctx.textAlign = 'center';
      ctx.fillStyle = '#ffdd33';
      const lineHeight = 18;
      for (let i = 0; i < cached.items.length; i++) {
        const it = cached.items[i];
        const name = REACTION_NAMES[it.type] || '';
        if (!name) continue;
        ctx.globalAlpha = Math.min(1, it.t);
        // 基础起点在头顶上方，每项之间上下错开一行
        const baseY = y - radius - 18;
        const rise  = (1.5 - it.t) * 20;
        const offset = i * lineHeight;
        ctx.fillText(name, x, baseY - rise - offset);
      }
      ctx.globalAlpha = 1.0;
    }
    this.enemyReactions.set(key, cached);
  }

  // ===== 子弹 =====
  private drawBullet(ctx: CanvasRenderingContext2D, cmd: RenderCommand): void {
    const { x, y, radius, r, g, b, a, bullet_strength } = cmd;

    // ---- Strong(2) / ExtraStrong(3) 专用：外层夸张火焰环 ----
    // 比普通子弹多：1) 更粗的径向渐变光晕  2) 8瓣火焰舌  3) 元素色外描边
    if (bullet_strength >= 2) {
      // 使用加色混合（lighter）让多层火焰叠加后更亮、更有发光感
      const prevComposite = ctx.globalCompositeOperation;
      ctx.globalCompositeOperation = 'lighter';

      // 1) 超大外层光晕（加亮 alpha）
      const bigGlow = ctx.createRadialGradient(x, y, 0, x, y, radius + 20);
      bigGlow.addColorStop(0,    `rgba(${r},${g},${b},0.90)`);
      bigGlow.addColorStop(0.30, `rgba(${r},${g},${b},0.55)`);
      bigGlow.addColorStop(0.65, `rgba(${r},${g},${b},0.25)`);
      bigGlow.addColorStop(1,    `rgba(${r},${g},${b},0)`);
      ctx.fillStyle = bigGlow;
      ctx.beginPath();
      ctx.arc(x, y, radius + 20, 0, Math.PI * 2);
      ctx.fill();

      // 2) 8 瓣火焰舌（锯齿环绕，视觉上像燃烧的火焰星芒）
      const tongues = 8;
      const innerR = radius + 5;
      const outerR = radius + 15;
      const phase = ((x * 0.13 + y * 0.07) % (Math.PI * 2));
      ctx.beginPath();
      for (let i = 0; i < tongues * 2; i++) {
        const ang = phase + (i * Math.PI) / tongues;
        const rr  = (i % 2 === 0) ? outerR : innerR;
        const px  = x + Math.cos(ang) * rr;
        const py  = y + Math.sin(ang) * rr;
        if (i === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py);
      }
      ctx.closePath();
      const flameGrad = ctx.createRadialGradient(x, y, radius, x, y, outerR);
      flameGrad.addColorStop(0, `rgba(${r},${g},${b},0.90)`);
      flameGrad.addColorStop(1, `rgba(${r},${g},${b},0)`);
      ctx.fillStyle = flameGrad;
      ctx.fill();

      // 恢复默认混合模式，避免影响核心绘制
      ctx.globalCompositeOperation = prevComposite;

      // 3) 元素色外描边（亮边+加粗，不使用 lighter 以免过曝）
      ctx.beginPath();
      ctx.arc(x, y, radius + 3, 0, Math.PI * 2);
      ctx.strokeStyle = `rgba(${r},${g},${b},1.0)`;
      ctx.lineWidth = 3;
      ctx.stroke();
    }

    // ---- 所有子弹共通：基础光晕 + 核心圆 ----
    const grad = ctx.createRadialGradient(x, y, 0, x, y, radius + 6);
    grad.addColorStop(0,   `rgba(${r},${g},${b},220)`);
    grad.addColorStop(0.6, `rgba(${r},${g},${b},140)`);
    grad.addColorStop(1,   'rgba(100,180,255,0)');
    ctx.fillStyle = grad;
    ctx.beginPath();
    ctx.arc(x, y, radius + 6, 0, Math.PI * 2);
    ctx.fill();

    ctx.beginPath();
    ctx.arc(x, y, radius, 0, Math.PI * 2);
    ctx.fillStyle = `rgba(${r},${g},${b},${a / 255})`;
    ctx.fill();

    // Strong：核心白色高光点（更亮更大）
    if (bullet_strength >= 2) {
      ctx.beginPath();
      ctx.arc(x, y, radius * 0.42, 0, Math.PI * 2);
      ctx.fillStyle = 'rgba(255,255,255,0.85)';
      ctx.fill();
    }
  }

  private drawUI(ctx: CanvasRenderingContext2D): void {
    // ===== 顶部操作说明 =====
    ctx.fillStyle = 'rgba(255,255,255,0.75)';
    ctx.font = '13px monospace';
    ctx.textAlign = 'left';
    const curElemName = ELEMENT_NAMES[this.currentElem] || '';
    ctx.fillText(`WASD: 移动   鼠标移动: 改变朝向   鼠标左键: 发射${curElemName}(弱)   鼠标右键: 发射${curElemName}(强·带火焰)`, 10, 22);
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