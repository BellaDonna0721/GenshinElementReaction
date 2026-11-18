import {
  RenderCommand, EntityType, ElementType,
  ELEMENT_COLORS, ELEMENT_NAMES, REACTION_NAMES, unpackGauge,
  HealthBarInfo, DamageNumber, WallRenderInfo, HitFx,
} from './types.js';

export class Renderer {
  private ctx: CanvasRenderingContext2D;
  private width: number;
  private height: number;

  // 反应飘字全局缓存（客户端管理渐隐 + 上浮动画）
  // 每项 { x, y, radius, type, t }：记录飘字生成时的位置，独立于敌人移动
  private reactionTexts: Array<{ x: number; y: number; radius: number; type: number; t: number }> = [];

  // 伤害飘字缓存（客户端管理渐隐 + 上浮动画）
  // 每项 { x, y, amount, r, g, b, t }：t 为剩余显示时长
  private damageNumbers: Array<{ x: number; y: number; amount: number; r: number; g: number; b: number; t: number; is_transformative: number }> = [];

  // 命中特效缓存（客户端管理三层动画生命周期）
  // 闪光：径向渐变快速放大淡出
  private hitFlashes: Array<{ x: number; y: number; r: number; g: number; b: number; t: number; maxT: number; scale: number }> = [];
  // 扩散光环：圆环从中心向外扩散，alpha 渐隐
  private hitRings: Array<{ x: number; y: number; r: number; g: number; b: number; t: number; maxT: number; scale: number }> = [];
  // 迸溅粒子：小颗粒向四周发散，减速+淡出
  private hitParticles: Array<{ x: number; y: number; vx: number; vy: number; r: number; g: number; b: number; t: number; maxT: number; size: number }> = [];

  // 子弹拖尾点池（跨帧渐隐，形成流体状拖尾）
  private bulletTrailDots: Array<{ x: number; y: number; r: number; g: number; b: number; t: number; maxT: number; size: number }> = [];

  // UI：左侧元素面板高亮的当前选中元素（默认 Pyro，和初始 spawn_player 一致）
  private currentElem: ElementType = ElementType.Pyro;

  // 敌人贴图：按元素类型（ElementType 1~7）索引，圆形裁剪后绘制
  private enemySprites: Array<HTMLImageElement | null> = [];
  private enemySpritesLoaded: boolean[] = [];

  // 玩家贴图（派蒙），圆形裁剪后绘制
  private playerSprite: HTMLImageElement | null = null;
  private playerSpriteLoaded: boolean = false;

  // 设置当前选中的元素（按键按下时由 main.ts 调用，立即更新白框高亮）
  setCurrentElement(elem: ElementType): void {
    this.currentElem = elem;
  }

  constructor(canvas: HTMLCanvasElement) {
    this.ctx = canvas.getContext('2d')!;
    this.width = canvas.width;
    this.height = canvas.height;

    // 预加载各元素史莱姆贴图（ElementType 1~7 对应 assets 下同名 jpg）
    const ENEMY_SPRITE_SRC: Record<number, string> = {
      [ElementType.Pyro]:    'assets/PyroSlime.jpg',
      [ElementType.Hydro]:   'assets/HydroSlime.jpg',
      [ElementType.Electro]: 'assets/ElectroSlime.jpg',
      [ElementType.Cryo]:    'assets/CryoSlime.jpg',
      [ElementType.Anemo]:   'assets/AnemoSlime.jpg',
      [ElementType.Geo]:     'assets/GeoSlime.jpg',
      [ElementType.Dendro]:  'assets/DendroSlime.jpg',
    };
    for (const [etStr, src] of Object.entries(ENEMY_SPRITE_SRC)) {
      const et = Number(etStr);
      const img = new Image();
      img.onload = () => { this.enemySpritesLoaded[et] = true; };
      img.src = src;
      this.enemySprites[et] = img;
    }

    // 预加载玩家贴图
    this.playerSprite = new Image();
    this.playerSprite.onload = () => { this.playerSpriteLoaded = true; };
    this.playerSprite.src = 'assets/paimon.png';
  }

  draw(commands: RenderCommand[], healthBars: HealthBarInfo[], damageNumbers: DamageNumber[], walls: WallRenderInfo[], hitFx: HitFx[], playerHpRatio: number, dt: number,
       bossPath?: Array<{ x: number; y: number }>,
       costmap?: Float32Array | null): void {
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

    // Costmap 热力图（调试用，很淡的半透明色块叠在网格上）
    if (costmap && costmap.length > 0) {
      this.drawCostmap(ctx, costmap);
    }

    // 墙体（在实体之下绘制，作为场景障碍物）
    this.drawWalls(ctx, walls);

    // 子弹（底层）+ 道具 + 敌人 + 玩家（顶层）分层画，防止遮挡
    const bullets: RenderCommand[] = [];
    const pickups: RenderCommand[] = [];
    const enemies: RenderCommand[] = [];
    const player: RenderCommand[] = [];
    for (const c of commands) {
      if (c.entity_type === EntityType.Bullet)        bullets.push(c);
      else if (c.entity_type === EntityType.Pickup)   pickups.push(c);
      else if (c.entity_type === EntityType.Enemy)    enemies.push(c);
      else                                            player.push(c);
    }
    // 子弹拖尾（在子弹本体之下绘制，形成身后流体光带）
    this.drawBulletTrails(ctx, bullets, dt);

    for (const c of bullets)   this.drawBullet(ctx, c);
    for (const c of pickups)    this.drawPickup(ctx, c);
    for (const c of enemies)     this.drawEnemy(ctx, c, dt);
    for (const c of player)      this.drawPlayer(ctx, c);

    // 血条（在实体之上绘制）
    this.drawHealthBars(ctx, healthBars);

    // 命中特效（闪光+扩散光环+迸溅粒子，在血条之上、伤害飘字之下）
    this.drawHitFx(ctx, hitFx, dt);

    // 伤害飘字（最顶层）
    this.drawDamageNumbers(ctx, damageNumbers, dt);

    // 反应飘字（渐隐 + 上浮，独立于敌人位置）
    this.drawReactionTexts(ctx, dt);

    // 玩家血条（HUD，左下角）
    this.drawPlayerHpBar(ctx, playerHpRatio);

    // Boss A* 规划路径（调试可视化，黄色虚线 +  waypoint 圆点）
    if (bossPath && bossPath.length >= 2) {
      this.drawBossPath(ctx, bossPath);
    }

    this.drawUI(ctx);
  }

  // ===== 墙体（矩形填充 + 描边）=====
  private drawWalls(ctx: CanvasRenderingContext2D, walls: WallRenderInfo[]): void {
    for (const w of walls) {
      const x = w.x - w.width / 2;
      const y = w.y - w.height / 2;

      // 填充
      ctx.fillStyle = `rgba(${w.r},${w.g},${w.b},${w.a / 255})`;
      ctx.fillRect(x, y, w.width, w.height);

      // 描边（略亮的边缘）
      ctx.strokeStyle = 'rgba(255,255,255,0.25)';
      ctx.lineWidth = 1.5;
      ctx.strokeRect(x, y, w.width, w.height);
    }
  }

  // ===== 血条 =====
  private drawHealthBars(ctx: CanvasRenderingContext2D, bars: HealthBarInfo[]): void {
    const barH = 5;
    for (const bar of bars) {
      const ratio = unpackGauge(bar.ratio);
      const bx = bar.x - bar.width / 2;
      const by = bar.y;

      // 背景
      ctx.fillStyle = '#333';
      ctx.fillRect(bx, by, bar.width, barH);

      // 前景（血量）
      // 颜色随血量变化：绿 → 黄 → 红
      let hpColor: string;
      if (ratio > 0.5)      hpColor = '#4ade80';  // 绿
      else if (ratio > 0.25) hpColor = '#facc15';  // 黄
      else                   hpColor = '#ef4444';  // 红
      ctx.fillStyle = hpColor;
      ctx.fillRect(bx, by, bar.width * ratio, barH);

      // 边框
      ctx.strokeStyle = 'rgba(0,0,0,0.6)';
      ctx.lineWidth = 1;
      ctx.strokeRect(bx, by, bar.width, barH);

      // 临时：血条上方显示敌人类型名（后续替换素材后移除）
      const typeName = this.getEnemyTypeName(bar.enemy_type);
      if (typeName) {
        ctx.font = '11px sans-serif';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'bottom';
        ctx.fillStyle = '#fff';
        ctx.strokeStyle = 'rgba(0,0,0,0.8)';
        ctx.lineWidth = 3;
        ctx.strokeText(typeName, bar.x, by - 2);
        ctx.fillText(typeName, bar.x, by - 2);
      }
    }
  }

  // 临时：敌人类型 ID → 中文名称（后续替换素材后移除）
  private getEnemyTypeName(typeId: number): string {
    switch (typeId) {
      case 1: return '火史莱姆';   // Pyro
      case 2: return '水史莱姆';   // Hydro
      case 3: return '雷Boss';     // Electro
      case 4: return '冰史莱姆';   // Cryo
      case 5: return '风史莱姆';   // Anemo
      case 6: return '岩史莱姆';   // Geo
      case 7: return '草史莱姆';   // Dendro
      default: return '';
    }
  }

  // ===== 玩家血条（HUD，左下角）=====
  private drawPlayerHpBar(ctx: CanvasRenderingContext2D, ratio: number): void {
    const barW = 180;
    const barH = 14;
    const bx = 16;
    const by = this.height - 30;

    // 背景
    ctx.fillStyle = 'rgba(0,0,0,0.6)';
    ctx.fillRect(bx - 2, by - 2, barW + 4, barH + 4);

    // 血条底色
    ctx.fillStyle = '#333';
    ctx.fillRect(bx, by, barW, barH);

    // 血条前景（颜色随血量变化）
    let hpColor: string;
    if (ratio > 0.5)       hpColor = '#4ade80';  // 绿
    else if (ratio > 0.25) hpColor = '#facc15';  // 黄
    else                   hpColor = '#ef4444';  // 红
    ctx.fillStyle = hpColor;
    ctx.fillRect(bx, by, barW * Math.max(0, ratio), barH);

    // 边框
    ctx.strokeStyle = '#fff';
    ctx.lineWidth = 1;
    ctx.strokeRect(bx, by, barW, barH);

    // 文字：HP 百分比
    ctx.font = 'bold 11px sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillStyle = '#fff';
    ctx.strokeStyle = 'rgba(0,0,0,0.8)';
    ctx.lineWidth = 3;
    const text = `HP ${Math.round(ratio * 100)}%`;
    ctx.strokeText(text, bx + barW / 2, by + barH / 2);
    ctx.fillText(text, bx + barW / 2, by + barH / 2);
  }

  // ===== 伤害飘字（客户端管理渐隐 + 上浮） =====
  private drawDamageNumbers(ctx: CanvasRenderingContext2D, newNumbers: DamageNumber[], dt: number): void {
    const dtFrame = dt;

    // 1) 按位置分组：同帧同位置的直伤居中大字，剧变在直伤右侧小字
    const posGroups = new Map<string, DamageNumber[]>();
    for (const dn of newNumbers) {
      const key = `${dn.x.toFixed(0)},${dn.y.toFixed(0)}`;
      if (!posGroups.has(key)) posGroups.set(key, []);
      posGroups.get(key)!.push(dn);
    }

    for (const [, group] of posGroups) {
      const hasDirect = group.some(d => d.is_transformative === 0);
      for (const dn of group) {
        let offsetX = 0;
        if (dn.is_transformative === 1) {
          // 剧变伤害：若同位置有直伤，偏移到右侧；否则居中
          offsetX = hasDirect ? 26 : 0;
        }
        this.damageNumbers.push({
          x: dn.x + offsetX, y: dn.y,
          amount: dn.amount,
          r: dn.r, g: dn.g, b: dn.b,
          t: 1.0,
          is_transformative: dn.is_transformative,
        });
      }
    }

    // 2) 每帧扣减剩余时长，过滤已过期
    this.damageNumbers = this.damageNumbers
      .map(d => ({ ...d, t: d.t - dtFrame }))
      .filter(d => d.t > 0);

    // 3) 绘制：上浮 + 渐隐；直伤大字居中，剧变小字
    ctx.textAlign = 'center';
    for (const d of this.damageNumbers) {
      const rise = (1.0 - d.t) * 30;  // 向上飘 30px
      ctx.globalAlpha = Math.min(1, d.t);
      ctx.fillStyle = `rgb(${d.r},${d.g},${d.b})`;
      if (d.is_transformative === 1) {
        ctx.font = 'bold 11px monospace';  // 剧变：小字号
      } else {
        ctx.font = 'bold 16px monospace';  // 直伤/增幅：大字号
      }
      ctx.fillText(String(d.amount), d.x, d.y - 20 - rise);
    }
    ctx.globalAlpha = 1.0;
  }

  // ===== 反应飘字（渐隐 + 上浮，独立于敌人位置） =====
  private drawReactionTexts(ctx: CanvasRenderingContext2D, dt: number): void {
    // 1) 每帧扣减剩余时长，过滤已过期
    this.reactionTexts = this.reactionTexts
      .map(it => ({ ...it, t: it.t - dt }))
      .filter(it => it.t > 0);

    // 2) 绘制：统一显示在生成位置的右侧，向上浮动 + 渐隐
    if (this.reactionTexts.length === 0) return;

    ctx.font = 'bold 14px monospace';
    ctx.textAlign = 'left';
    ctx.fillStyle = '#ffdd33';

    for (const it of this.reactionTexts) {
      const name = REACTION_NAMES[it.type] || '';
      if (!name) continue;
      const rise = (1.5 - it.t) * 20;  // 向上飘 20px
      ctx.globalAlpha = Math.min(1, it.t);
      ctx.fillText(name, it.x + it.radius + 8, it.y - rise);
    }
    ctx.globalAlpha = 1.0;
  }

  // ===== 命中特效：闪光 + 扩散光环 + 迸溅粒子（客户端管理生命周期）=====
  private drawHitFx(ctx: CanvasRenderingContext2D, newFx: HitFx[], dt: number): void {
    const dtFrame = dt;

    // 1) 从本帧新命中数据生成三层特效对象
    for (const fx of newFx) {
      const { x, y, r, g, b, hit_type, scale } = fx;

      // 闪光：径向渐变快速放大淡出（0.2s）
      this.hitFlashes.push({ x, y, r, g, b, t: 0.20, maxT: 0.20, scale });

      // 扩散光环：圆环从中心向外扩散（0.35s）
      this.hitRings.push({ x, y, r, g, b, t: 0.35, maxT: 0.35, scale });

      // 迸溅粒子：8~12 颗向四周发散（0.4s）
      // 命中敌人时粒子多一些，撞墙时少一些
      const count = hit_type === 0 ? 10 : 7;
      for (let i = 0; i < count; i++) {
        const ang = (Math.PI * 2 * i) / count + (Math.random() - 0.5) * 0.6;
        const speed = 70 + Math.random() * 70;  // 70~140 px/s
        this.hitParticles.push({
          x, y,
          vx: Math.cos(ang) * speed,
          vy: Math.sin(ang) * speed,
          r, g, b,
          t: 0.40, maxT: 0.40,
          size: 2 + Math.random() * 2,  // 2~4 px
        });
      }
    }

    // 2) 更新所有活跃特效（扣减寿命、粒子位移+减速）
    this.hitFlashes = this.hitFlashes.map(f => ({ ...f, t: f.t - dtFrame })).filter(f => f.t > 0);
    this.hitRings   = this.hitRings.map(f => ({ ...f, t: f.t - dtFrame })).filter(f => f.t > 0);
    this.hitParticles = this.hitParticles
      .map(p => ({
        ...p,
        t: p.t - dtFrame,
        x: p.x + p.vx * dtFrame,
        y: p.y + p.vy * dtFrame,
        vx: p.vx * 0.92,  // 摩擦减速
        vy: p.vy * 0.92,
      }))
      .filter(p => p.t > 0);

    // 敌人实际半径 = scale * 20（scale 由 radius/20 传入）
    // 闪光与光环均以敌人半径为基准放大，确保扩散到身体之外清晰可见
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';  // 叠加发光，避免被敌人贴图吃掉

    // 3) 绘制：闪光（底层）→ 光环 → 粒子（顶层）
    // 闪光：径向渐变快速放大淡出（0.2s），从敌人半径0.3倍扩散到1.6倍
    for (const f of this.hitFlashes) {
      const prog = 1 - f.t / f.maxT;  // 0→1
      const baseR = f.scale * 20;  // 敌人半径
      const radius = baseR * (0.3 + prog * 1.3);  // 0.3R → 1.6R
      const alpha = (1 - prog) * 0.85;
      const grad = ctx.createRadialGradient(f.x, f.y, 0, f.x, f.y, radius);
      grad.addColorStop(0,   `rgba(255,255,255,${alpha})`);          // 中心白亮
      grad.addColorStop(0.3, `rgba(${f.r},${f.g},${f.b},${alpha})`);  // 中段元素色
      grad.addColorStop(1,   `rgba(${f.r},${f.g},${f.b},0)`);         // 边缘透明
      ctx.fillStyle = grad;
      ctx.beginPath();
      ctx.arc(f.x, f.y, radius, 0, Math.PI * 2);
      ctx.fill();
    }

    // 扩散光环：圆环从敌人半径0.6倍扩散到2.0倍（0.35s）
    for (const f of this.hitRings) {
      const prog = 1 - f.t / f.maxT;  // 0→1
      const baseR = f.scale * 20;  // 敌人半径
      const radius = baseR * (0.6 + prog * 1.4);  // 0.6R → 2.0R
      const alpha = (1 - prog) * 0.9;
      const lineW = 4 - prog * 2.5;  // 4 → 1.5
      ctx.beginPath();
      ctx.arc(f.x, f.y, radius, 0, Math.PI * 2);
      ctx.strokeStyle = `rgba(${f.r},${f.g},${f.b},${alpha})`;
      ctx.lineWidth = lineW;
      ctx.stroke();
    }

    // 迸溅粒子：小圆点，随寿命淡出
    for (const p of this.hitParticles) {
      const alpha = Math.min(1, p.t / p.maxT * 1.5);
      ctx.beginPath();
      ctx.arc(p.x, p.y, p.size, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(${p.r},${p.g},${p.b},${alpha})`;
      ctx.fill();
    }

    ctx.restore();
  }

  // ===== 道具（结晶晶片等） =====
  private drawPickup(ctx: CanvasRenderingContext2D, cmd: RenderCommand): void {
    const { x, y, radius, r, g, b, a } = cmd;

    // 外发光光晕
    const glow = ctx.createRadialGradient(x, y, 0, x, y, radius + 8);
    glow.addColorStop(0,   `rgba(${r},${g},${b},0.5)`);
    glow.addColorStop(1,   `rgba(${r},${g},${b},0)`);
    ctx.fillStyle = glow;
    ctx.beginPath();
    ctx.arc(x, y, radius + 8, 0, Math.PI * 2);
    ctx.fill();

    // 本体：实心圆 + 白色描边
    ctx.beginPath();
    ctx.arc(x, y, radius, 0, Math.PI * 2);
    ctx.fillStyle = `rgba(${r},${g},${b},${a / 255})`;
    ctx.fill();
    ctx.strokeStyle = 'rgba(255,255,255,0.7)';
    ctx.lineWidth = 1.5;
    ctx.stroke();

    // 中心高光点
    ctx.beginPath();
    ctx.arc(x, y, radius * 0.35, 0, Math.PI * 2);
    ctx.fillStyle = 'rgba(255,255,255,0.6)';
    ctx.fill();
  }

  // ===== 玩家（含朝向箭头 + 护盾圆圈） =====
  private drawPlayer(ctx: CanvasRenderingContext2D, cmd: RenderCommand): void {
    const { x, y, radius, r, g, b, a, facing_dx, facing_dy, bullet_strength } = cmd;

    // 护盾圆圈：bullet_strength 复用为护盾层数（0=无，1~5=层数）
    // 土黄色半透明圆环，环绕玩家身体外侧
    if (bullet_strength > 0) {
      ctx.save();
      ctx.beginPath();
      ctx.arc(x, y, radius + 6, 0, Math.PI * 2);
      ctx.strokeStyle = 'rgba(200, 170, 80, 0.85)';
      ctx.lineWidth = 3;
      ctx.stroke();
      // 内层淡色填充，体现护盾质感
      ctx.beginPath();
      ctx.arc(x, y, radius + 6, 0, Math.PI * 2);
      ctx.fillStyle = 'rgba(200, 170, 80, 0.12)';
      ctx.fill();
      ctx.restore();
    }

    // 身体：贴图优先（圆形裁剪），加载失败则回退到原圆形色块
    if (this.playerSpriteLoaded && this.playerSprite) {
      ctx.save();
      ctx.imageSmoothingEnabled = true;
      ctx.imageSmoothingQuality = 'high';
      ctx.beginPath();
      ctx.arc(x, y, radius, 0, Math.PI * 2);
      ctx.closePath();
      ctx.clip();
      const size = radius * 2;
      ctx.drawImage(this.playerSprite, x - radius, y - radius, size, size);
      ctx.restore();
    } else {
      // 回退：纯色圆形 + 白描边
      ctx.beginPath();
      ctx.arc(x, y, radius, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(${r},${g},${b},${a / 255})`;
      ctx.fill();
      ctx.strokeStyle = '#ffffff';
      ctx.lineWidth = 2;
      ctx.stroke();
    }

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
  private drawEnemy(ctx: CanvasRenderingContext2D, cmd: RenderCommand, dt: number): void {
    const { x, y, radius, r, g, b, a,
            element1_type, element2_type,
            element1_gauge, element2_gauge,
            reaction_mask_lo, reaction_mask_hi,
            bullet_strength, enemy_type } = cmd;
    const isFrozen = bullet_strength === 1;  // 冻结标志（复用 bullet_strength 字段）
    const FROZEN_COLOR = '#BFFFFF';          // 冻元素：更亮的淡蓝色，区分普通冰

    // 敌人本体元素类型（独立字段，C++ 端从 EnemyAI.enemy_type 写入）
    const enemyType = enemy_type;
    const sprite = this.enemySprites[enemyType] || null;
    const spriteLoaded = this.enemySpritesLoaded[enemyType] === true;

    // 身体：贴图优先（圆形裁剪），加载失败则回退到原圆形色块
    if (spriteLoaded && sprite) {
      ctx.save();
      ctx.imageSmoothingEnabled = true;
      ctx.imageSmoothingQuality = 'high';
      ctx.beginPath();
      ctx.arc(x, y, radius, 0, Math.PI * 2);
      ctx.closePath();
      ctx.clip();
      // 保持贴图原始宽高比，在圆内缩放到"至少一个维度填满"，超出部分由 clip 裁掉
      // 竖图（立绘）宽度方向填满、上下溢出被裁；横图高度方向填满、左右溢出被裁
      const maxSize = radius * 2;
      const natW = sprite.naturalWidth;
      const natH = sprite.naturalHeight;
      const scale = Math.max(maxSize / natW, maxSize / natH);
      const drawW = natW * scale;
      const drawH = natH * scale;
      ctx.drawImage(sprite, x - drawW / 2, y - drawH / 2, drawW, drawH);
      ctx.restore();
    } else {
      // 回退：纯色圆形
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

    // 冰冻覆盖：冻结时给敌人身体叠一层淡蓝色半透明
    if (isFrozen) {
      ctx.save();
      ctx.beginPath();
      ctx.arc(x, y, radius, 0, Math.PI * 2);
      ctx.fillStyle = 'rgba(180, 240, 255, 0.35)';
      ctx.fill();
      ctx.restore();
    }

    // ---------- 元素光环：内圈（slot[0]） + 外圈（slot[1]，共存态）----------
    const g1 = unpackGauge(element1_gauge);
    const g2 = unpackGauge(element2_gauge);
    if (element1_type !== 0 && g1 > 0) {
      // 冻结时内圈是冻元素，用更亮的淡蓝色；否则用普通元素色
      const color = isFrozen ? FROZEN_COLOR : (ELEMENT_COLORS[element1_type] || '#888');
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
      // 单元素 → 完整长度（冻结时内圈是冻元素，用更亮的蓝色）
      const color = isFrozen ? FROZEN_COLOR : (ELEMENT_COLORS[element1_type] || '#888');
      ctx.fillStyle = color;
      ctx.fillRect(barX, barY, barW * g1, barH);
      ctx.fillStyle = color;
      ctx.font = 'bold 10px monospace';
      ctx.textAlign = 'left';
      ctx.fillText(isFrozen ? '冻' : (ELEMENT_NAMES[element1_type] || ''), barX, barY - 1);
    } else if (has1 && has2) {
      // 双元素 → 左半显示 slot0，右半显示 slot1（冻结时 slot0 是冻元素）
      const color1 = isFrozen ? FROZEN_COLOR : (ELEMENT_COLORS[element1_type] || '#888');
      const color2 = ELEMENT_COLORS[element2_type] || '#888';
      ctx.fillStyle = color1;
      ctx.fillRect(barX, barY, (barW / 2) * g1, barH);
      ctx.fillStyle = color2;
      ctx.fillRect(barX + barW / 2, barY, (barW / 2) * g2, barH);
      // 元素名：左名、右名
      ctx.font = 'bold 10px monospace';
      ctx.textAlign = 'left';
      ctx.fillStyle = color1;
      ctx.fillText(isFrozen ? '冻' : (ELEMENT_NAMES[element1_type] || ''), barX, barY - 1);
      ctx.textAlign = 'right';
      ctx.fillStyle = color2;
      ctx.fillText(ELEMENT_NAMES[element2_type] || '', barX + barW, barY - 1);
    } else if (has2 && !has1) {
      // 理论上不会出现（写 slot 时先写 0 再写 1），兜底
      const color = ELEMENT_COLORS[element2_type] || '#888';
      ctx.fillStyle = color;
      ctx.fillRect(barX, barY, barW * g2, barH);
    }

    // 反应文字标签：把本帧新反应 push 到全局数组，记录生成时的位置
    // （不依赖敌人当前位置，避免敌人移动导致飘字丢失）
    const mask: number = (reaction_mask_lo & 0xFF) | ((reaction_mask_hi & 0xFF) << 8);
    for (let i = 1; i <= 16; i++) {
      if ((mask & (1 << i)) !== 0) {
        this.reactionTexts.push({ x, y, radius, type: i, t: 1.5 });
      }
    }
  }

  // ===== 子弹飞行拖尾（流体状渐变光带） =====
  // 跨帧点池：每帧在子弹头部生成新点，旧点随寿命递减淡出，形成连续流体拖尾
  private drawBulletTrails(ctx: CanvasRenderingContext2D, bullets: RenderCommand[], dt: number): void {
    const dtFrame = dt;

    // 1) 为本帧每个子弹在头部生成新拖尾点
    for (const bullet of bullets) {
      const { x, y, radius, r, g, b, facing_dx, facing_dy } = bullet;
      if (facing_dx === 0 && facing_dy === 0) continue;

      // 在子弹头部沿反方向生成拖尾点，头部宽、尾部窄（锥形）
      const tx = -facing_dx;
      const ty = -facing_dy;
      for (let k = 0; k < 3; k++) {
        const offset = k * 2;  // 0, 2, 4 px（原 0~8px，现 0~4px，长度减半）
        // 头部点明显大于子弹半径，向尾部线性收窄，确保与子弹本体平滑衔接
        const sizeRatio = 1.6 - k * 0.48;  // 1.6 → 0.64（3个点线性收窄）
        this.bulletTrailDots.push({
          x: x + tx * offset,
          y: y + ty * offset,
          r, g, b,
          t: 0.14, maxT: 0.14,           // 单颗拖尾点寿命 0.14s（减半）
          size: radius * sizeRatio,
        });
      }
    }

    // 2) 更新所有拖尾点：扣减寿命，过滤过期
    this.bulletTrailDots = this.bulletTrailDots
      .map(p => ({ ...p, t: p.t - dtFrame }))
      .filter(p => p.t > 0);

    // 3) 绘制所有拖尾点（lighter 叠加发光，形成连续流体光带）
    ctx.save();
    ctx.globalCompositeOperation = 'lighter';
    for (const p of this.bulletTrailDots) {
      const fade = p.t / p.maxT;          // 1 → 0
      const alpha = fade * 0.55;
      const size  = p.size * (0.3 + fade * 0.7);
      ctx.beginPath();
      ctx.arc(p.x, p.y, size, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(${p.r},${p.g},${p.b},${alpha})`;
      ctx.fill();
    }
    ctx.restore();
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
    // ===== 左侧：数字键 ↔ 元素映射面板 =====
    const panelX = 18;                // 面板左上 X
    const panelY = 18;                // 面板左上 Y
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
    ctx.textBaseline = 'middle'; // 统一以行中线为基准，避免数字/横线/文字基线错位
    for (let i = 0; i < entries.length; i++) {
      const { key, elem } = entries[i];
      const centerY = panelY + i * rowH + rowH / 2; // 每行垂直中心
      const rowW = 142;
      const name = ELEMENT_NAMES[elem] || '';
      const color = ELEMENT_COLORS[elem] || '#888';

      if (elem === this.currentElem) {
        const rectH = rowH - 2;
        ctx.fillStyle = 'rgba(255,255,255,0.08)';
        ctx.fillRect(panelX, centerY - rectH / 2, rowW, rectH);
        ctx.strokeStyle = '#ffffff';
        ctx.lineWidth = 2;
        ctx.strokeRect(panelX, centerY - rectH / 2, rowW, rectH);
      }

      ctx.fillStyle = '#ffffff';
      ctx.textAlign = 'left';
      ctx.fillText(key, panelX + 8, centerY);

      ctx.fillStyle = 'rgba(255,255,255,0.4)';
      ctx.fillText('-', panelX + 28, centerY);

      const sqX = panelX + 44;
      const sqSize = 14;
      ctx.fillStyle = color;
      ctx.fillRect(sqX, centerY - sqSize / 2, sqSize, sqSize);
      ctx.strokeStyle = 'rgba(255,255,255,0.5)';
      ctx.lineWidth = 1;
      ctx.strokeRect(sqX, centerY - sqSize / 2, sqSize, sqSize);

      ctx.fillStyle = color;
      ctx.fillText(name, panelX + 68, centerY);
    }
  }

  // ===== Costmap 热力图 =====
  // 每格 25px，代价 0→4+ 映射成透明红色色块叠在网格上
  private drawCostmap(ctx: CanvasRenderingContext2D, costmap: Float32Array): void {
    const cellSize = 25;
    const W = 40;
    const maxCost = 4.0;  // COSTMAP_CLOSE = 4.0

    for (let cy = 0; cy < 28; cy++) {
      for (let cx = 0; cx < W; cx++) {
        const idx = cy * W + cx;
        const v = costmap[idx];
        if (v < 0) continue;  // 墙，由 drawWalls 画
        if (v <= 0.01) continue;  // 无代价，不画

        // 代价 → alpha：离墙越近越红
        const t = Math.min(1, v / maxCost);
        const alpha = t * 0.35;  // 最大 35% 透明红
        ctx.fillStyle = `rgba(255, 80, 80, ${alpha.toFixed(3)})`;
        ctx.fillRect(cx * cellSize, cy * cellSize, cellSize, cellSize);
      }
    }
  }

  // ===== Boss A* 路径调试可视化 =====
  // 黄色虚线连接 waypoint + waypoint 圆点 + 终点高亮
  private drawBossPath(ctx: CanvasRenderingContext2D, path: Array<{ x: number; y: number }>): void {
    ctx.save();

    // 路径线：黄色虚线
    ctx.strokeStyle = '#ffeb3b';
    ctx.lineWidth = 2;
    ctx.setLineDash([8, 6]);
    ctx.beginPath();
    ctx.moveTo(path[0].x, path[0].y);
    for (let i = 1; i < path.length; i++) {
      ctx.lineTo(path[i].x, path[i].y);
    }
    ctx.stroke();
    ctx.setLineDash([]);

    // waypoint 圆点（跳过起点，只画中间点和终点）
    for (let i = 1; i < path.length; i++) {
      const p = path[i];
      ctx.beginPath();
      ctx.arc(p.x, p.y, 4, 0, Math.PI * 2);
      ctx.fillStyle = i === path.length - 1 ? '#4ade80' : '#ffeb3b';
      ctx.fill();
      ctx.strokeStyle = 'rgba(0,0,0,0.7)';
      ctx.lineWidth = 1;
      ctx.stroke();
    }

    ctx.restore();
  }
}