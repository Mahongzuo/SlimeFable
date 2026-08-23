# 3A 战斗与 Boss AI 优化 — 实施进度与交接文档

> 本文档面向接手该任务的 agent。原方案见仓库根 `Docs/Maps/Days/08/0815_TodayInHistory_Design.md` 中的 "3A 战斗与 Boss AI 优化方案" 章节，或向上一个 agent 索取。
> **路线决策**：严格按原方案全量重构（GAS 权威 + StateTree 决策 + Encounter Director）。
> **验收方式**：只做 UBT 编译 + 静态检查，不启动 PIE，战斗手感由用户手动验收。
> **沟通语言**：与用户用中文；代码标识符与引擎 API 保持英文。

## 0. 必读与硬约束

开工前必读（顺序不可颠倒）：

1. `AGENTS.md` — 项目入口
2. `.cursor/skills/slimefable-spec/SKILL.md` — 项目宪法
3. `.cursor/skills/slimefable-spec/references/toolchain.md` — UBT/编辑器/MCP 路径（**不要满盘搜 Build.bat**）
4. `.cursor/skills/slimefable-spec/references/coding-conventions.md` — Details 分类、ToolTip、命名

关键硬约束：

- 引擎 UE 5.8，根目录 `D:\Program Files\Epic Games\UE_5.8`
- UBT 命令：
  ```powershell
  & "D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" SlimeFableEditor Win64 Development -Project="E:\UE\SlimeFable\SlimeFable.uproject" -WaitMutex
  ```
- 改 C++ 时编辑器默认关着；新增 UCLASS/UFUNCTION 必须完整 UBT，不要指望 Live Coding
- MCP 串行调用，禁止并行叠 MCP；批量建关用 `Content/Python/create_day_levels.py`
- Details 字段用根类目 `0_Config`，可调字段必须有中文 ToolTip
- 不主动写用户未要的 Markdown；本文件是用户明确要的交接文档

## 1. 总体进度

**约 30% 完成。步骤 1–2 的代码骨架已写入但尚未编译验证。**

按原方案"实施顺序"的 10 步编号：

| 步骤 | 内容 | 状态 |
|------|------|------|
| 1 | GAS 模块、敌人 ASC、AttributeSet、旧生命系统同步层 | **代码已写，未编译** |
| 2 | 统一 Gameplay Tags、伤害/硬直/死亡 GameplayEffect | **代码已写，未编译** |
| 3 | EnemyCombatComponent 的 Melee/Projectile/AoE/Dash 迁移为 GAS Ability | 未开始 |
| 4 | 共享 StateTree 决策层（7 Task + 3 Evaluator） | 未开始 |
| 5 | AEnemyEncounterDirector + UEnemyEncounterDefinition + 攻击预算 + 站位分配 | 部分（仅最小版 Subsystem，见下） |
| 6 | 四类角色行为差异化（扑空失衡/格挡反击/换位压制/指挥调度） | 未开始 |
| 7 | 天皇三阶段 Boss 配置 | 未开始 |
| 8 | 1945 场地广播节点、增援顺序、完成奖励 | 未开始 |
| 9 | 清理旧 Fighter AI / Tower 重复路径为兼容层 | 未开始 |
| 10 | 完整 UBT 编译 + 静态检查 + 1945 手动验收 | 未开始 |

## 2. 已完成内容（详细）

### 2.1 GAS 模块接入（步骤 1 基础）

- `SlimeFable.uproject`：启用 `GameplayAbilities` 插件
- `Source/SlimeFable/SlimeFable.Build.cs`：模块依赖加 `GameplayAbilities`、`GameplayTasks`
- `AEnemyCharacter` 拥有 `UAbilitySystemComponent AbilitySystem` + `UEnemyAttributeSet EnemyAttributes`
- `BeginPlay` 末尾调用 `InitAbilitySystem()`：初始化 AbilityActorInfo、按 `MaxHP/MaxPoise/Guard/MoveSpeed` 填属性、给 ASC 加角色 Tag、绑定 `Health->OnHealthChanged` 回调做门面同步

### 2.2 Native Gameplay Tags（步骤 2）

**新增文件**：
- `Source/SlimeFable/Enemy/SlimeEnemyGameplayTags.h`
- `Source/SlimeFable/Enemy/SlimeEnemyGameplayTags.cpp`

声明方式：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` / `UE_DEFINE_GAMEPLAY_TAG`（C++ 与 Blueprint 共享同一来源）。

已声明 Tag：

- State：`Combat.State.Alert / Telegraph / Attacking / Recover / Staggered / Guarding / Invulnerable / SuperArmor / Whiffed`
- Event：`Combat.Event.Hit / GuardBreak / PhaseChanged / SupportCalled / BossExposed`
- Role：`Combat.Role.Chaser / Duelist / Suppressor / Commander`
- Data（SetByCaller 通道）：`Combat.Data.Damage / PoiseDamage / Healing / Duration / Power`
- Ability 激活通道：`Combat.Ability.Skill`

辅助函数 `SlimeEnemyTags::RoleTag(uint8 Role)` 把 `EEnemyCombatRole` 转成对应 Tag。

**注意**：`Config/DefaultGameplayTags.ini` 里原先手写的 `Combat.State.* / Combat.Event.* / Combat.Role.*` 条目已被删除，改由 native tag 注册。ini 里只保留 `Combat / Combat.Skill / Combat.Damage / Combat.Status / Combat.Reaction` 这些非 enemy 战斗 tag。

### 2.3 AttributeSet（步骤 1）

**修改文件**：
- `Source/SlimeFable/Enemy/EnemyAttributeSet.h`（重写）
- `Source/SlimeFable/Enemy/EnemyAttributeSet.cpp`（重写）

属性：
- 真实属性：`Health / MaxHealth / Poise / MaxPoise / Guard / MoveSpeed / DamagePower / PhaseIndex`
- Meta 属性（由 GE 写入，在 `PostGameplayEffectExecute` 消费）：`IncomingDamage / IncomingPoiseDamage / IncomingHealing`

`PostGameplayEffectExecute` 逻辑：
- `IncomingDamage` → 扣 `Health`，调 `AEnemyCharacter::OnGasDamageApplied`（转发到旧 Health 门面）
- `IncomingHealing` → 加 `Health`，调 `OnGasHealingApplied`
- `IncomingPoiseDamage` → 扣 `Poise`，归零时调 `OnPoiseBroken`（触发失衡）
- `PreAttributeChange`：Health clamp 到 `[0, MaxHealth]`，Poise clamp 到 `[0, MaxPoise]`

### 2.4 GameplayEffect 类（步骤 2）

**新增文件**：
- `Source/SlimeFable/Enemy/EnemyGameplayEffects.h`
- `Source/SlimeFable/Enemy/EnemyGameplayEffects.cpp`

C++ 构造的 GE 类（无需 .uasset）：

| 类 | 用途 | Magnitude 通道 |
|----|------|----------------|
| `UGE_EnemyDamage` | Instant 伤害 + 硬直伤害 | `Data.Damage` / `Data.PoiseDamage` |
| `UGE_EnemyHealing` | Instant 治疗 | `Data.Healing` |
| `UGE_EnemyTimedState` | 抽象基类，HasDuration，duration 由 `Data.Duration` SetByCaller | — |
| `UGE_EnemyStagger` | 失衡：授予 `State.Staggered` | `Data.Duration` |
| `UGE_EnemyInvulnerable` | 无敌：授予 `State.Invulnerable` | `Data.Duration` |
| `UGE_EnemyGuard` | 格挡：授予 `State.Guarding` | `Data.Duration` |
| `UGE_EnemySuperArmor` | 霸体：授予 `State.SuperArmor` | `Data.Duration` |
| `UGE_EnemyEmpower` | 指挥者强化：`DamagePower` 乘算 | `Data.Duration` / `Data.Power` |

Tag 授予通过 `UTargetTagsGameplayEffectComponent::SetAndApplyTargetTagChanges`。

### 2.5 AEnemyCharacter GAS 权威化（步骤 1–2）

**修改文件**：
- `Source/SlimeFable/Enemy/EnemyCharacter.h`
- `Source/SlimeFable/Enemy/EnemyCharacter.cpp`

新增 API：

- `ApplyTimedState(EffectClass, Duration, Power)` — 通用定时状态 GE 应用入口
- `HasCombatStateTag(Tag)` / `IsStaggered()` / `GetPoisePercent()` / `GetHealthPercent()`
- `EnterStagger(Duration, Instigator)` — 中断当前攻击、释放攻击权、应用 `UGE_EnemyStagger`、回满 Poise、广播 `Event.Hit`
- `OnGasDamageApplied / OnGasHealingApplied / OnPoiseBroken / OnGuardBroken` — AttributeSet 回调
- `HandleHealthFacadeChanged(CurrentHP, MaxHP)` — 旧 Health 组件变更回写 GAS（防双向循环用 `bSyncingHealthFacade`）
- `InitAbilitySystem()` / `TickPoiseRegen(DeltaSeconds)`

`ApplyDamage` 重写为 GAS 权威路径：

1. 若 `bRoutingGasDamage` 或无 ASC → 走旧 `Health->ApplyDamage`（兼容早期 spawn / CDO）
2. `State.Invulnerable` → 直接 return
3. `State.Guarding` → 伤害乘 `1 - GuardDamageReduction`，扣 `Guard`，Guard 归零触发 `OnGuardBroken`
4. `State.SuperArmor` → 不产生 Poise 伤害
5. 构造 `UGE_EnemyDamage` Spec，SetByCaller 设 `Data.Damage` / `Data.PoiseDamage`，`ApplyGameplayEffectSpecToSelf`
6. `PostGameplayEffectExecute` 消费 meta 属性 → 调 `OnGasDamageApplied` → 用 `bRoutingGasDamage` 守卫回写旧 Health 组件

新增 `0_Config|Combat` 字段（均有中文 ToolTip）：

- `PoiseDamageRatio = 0.6` — 每点伤害转硬直伤害
- `PoiseRegenPerSecond = 12` — 脱战硬直回复
- `StaggerDuration = 1.2` — 失衡时长 = 反击窗口
- `GuardDamageReduction = 0.65` — 格挡减伤

### 2.6 既有已完成（上一个 agent 留下，本 agent 接手前已存在）

这些在 git 工作区里已存在，**不是本 agent 本次写的**，但属于本任务范围：

- `EnemyEncounterSubsystem.h/.cpp` — 最小版攻击权系统（WorldSubsystem，按角色各 1 个名额、超时回收、`GetBossPhase` 按天皇血量算 1/2/3）
- `AEnemyCharacter` 上的 `CombatRole / MaxPoise / Guard` 字段、`RequestAttackSlot / ReleaseAttackSlot / GetEncounterPhase`
- `BeginPlay` 按 BP 类名（Watchdog/Samurai/Gunner/Emperor）自动赋 `CombatRole`
- `AEnemyFighterAIController::BeginExecute` 接 `RequestAttackSlot`，失败则回 Combat 状态换位
- `AEnemyTower::ConfigureMobileGunner / TickMobileGunner` — 机枪手落地、Walking 模式、360 速度、推进、`bDevourable=true`
- `IsDevourableNow` 移除了对 Tower 的一票否决

## 3. 未完成内容（按实施顺序）

### 步骤 1–2 剩余

- **TickPoiseRegen 未接入 Tick**：`AEnemyCharacter::Tick` 目前只调 `TickOutOfCombatReset`，没调 `TickPoiseRegen`。需要在 Tick 里加一行。
- **未编译验证**：本次写的所有代码尚未跑 UBT。下一步 agent 应先跑一次完整 UBT，修掉编译错误再继续。
- **死亡未走 GAS**：`HandleDeath` 仍空，`HandleDied` 走旧逻辑。方案要求"死亡统一走 GAS"，但目前死亡路径未改。建议在 `OnGasDamageApplied` 里检测 `Health->IsAlive() == false` 后走 `HandleDied`，或在 `PostGameplayEffectExecute` 里 Health 归零时直接触发。

### 步骤 3：EnemyCombatComponent 迁移为 GAS Ability

现状：`UEnemyCombatComponent` 仍用 `FEnemySkillDef` + 手动 `TickAction` 时间轴驱动 Windup/HitStart/HitEnd/Recovery、手动 `FireHit / ExecuteDash / ExecuteProjectile`。

要做：

- 新建 `UEnemyGameplayAbility`（建议放 `Source/SlimeFable/Enemy/Abilities/`）
- 一个通用 `UEnemySkillAbility`，从 `FEnemySkillDef` 生成 Spec，用 `PlayMontageAndWait` + `AbilityTask_Delay` + 命中帧 `GameplayCue` + `UGameplayAbility::ApplyEffect` 投 `UGE_EnemyDamage`
- Melee / Projectile / AoE / Dash 四种 Exec 各一个子类或一个参数化基类
- `FEnemyMoveDef` 升级为 `FEnemyAbilityDef`（含 Cooldown GE、Telegraph Cue、阶段/角色/攻击权条件）
- `EnemyCombatComponent` 保留为兼容门面：`TryExecute(FEnemySkillDef)` 内部转成 `AbilitySystem->TryActivateAbility`
- 玩家 Morph 时的 `PollPlayerCombatKeys` 路径要保留（MorphTarget 用玩家输入驱动 Ability）

### 步骤 4：共享 StateTree 决策层

现状：`AEnemyFighterAIController` 每帧硬编码 `Idle/Combat/Choose/Telegraph/Execute/Recover` 状态机 + 权重随机 `SelectMove`。

要做：

- 新建 `UEnemyStateTree` 资产（C++ 不便建资产，可先写 native Task/Evaluator，资产由无头 Python 或手动建）
- State：`Dormant → Alert → AcquireTarget → Reposition → ChooseAbility → Telegraph → Execute → Recover → React → Dead`
- 新增 Task（建议放 `Source/SlimeFable/Enemy/StateTree/`）：
  - `STTask_FindCombatTarget`
  - `STTask_RequestAttackSlot`
  - `STTask_MoveToCombatPosition`
  - `STTask_TelegraphAbility`
  - `STTask_ActivateEnemyAbility`
  - `STTask_RecoverFromAttack`
  - `STTask_ReactToCombatEvent`
- 新增 Evaluator：
  - `STEvaluator_EnemyThreat`
  - `STEvaluator_BossPhase`
  - `STEvaluator_EncounterPressure`
- `AEnemyFighterAIController` 改为持有 `UStateTreeComponent`，旧 Tick 状态机降级为无 StateTree 资产时的回退
- 选招依据改为方案里的 7 步排序（目标状态→距离视线→队友行为→遭遇压力→CD→上次结果→权重随机）

### 步骤 5：Encounter Director

现状：只有 `UEnemyEncounterSubsystem`（WorldSubsystem，最小版攻击权 + GetBossPhase）。

要做：

- 新建 `AEnemyEncounterDirector`（Actor，放 1945 关卡），引用 `UEnemyEncounterDefinition`
- 新建数据资产：
  - `UEnemyEncounterDefinition` — 整场遭遇配置
  - `FEnemyEncounterPhaseDef` — 阶段定义（血量阈值、可用 Ability、增援、场地规则）
  - `FEnemyAbilityDef` — 单个 Ability 的距离/视线/阶段/角色/攻击权条件
- Director 职责：
  - 自动发现带 `Combat.Role.*` Tag 的敌人
  - 注册 Boss / 支援单位 / 场地节点
  - 管理阶段切换、广播 `Event.PhaseChanged / SupportCalled / BossExposed`
  - 维护威胁表
  - 限制同时攻击数（现 Subsystem 已有，可迁入 Director）
  - 分配近战位 / 远程位 / 包抄位 / 撤退位
- 攻击预算（方案默认值）：
  - Boss 主攻 1 / 近战支援 1 / 远程压制 1
  - 同一敌人受击后短时不能被另一敌人重复硬直

### 步骤 6：四类角色行为差异化

现状：四类敌人行为无差别，只是 `CombatRole` enum 值不同。

要做（按方案"敌人职责"）：

- **看门狗 Chaser/Punisher**：追逐、扑击、惩罚远离队伍的玩家；扑空后进入 `State.Whiffed` 失衡（已有 Tag，未用）。`FillWatchdogBiteMoves` 已有 3 招，需加扑空检测。
- **武士 Duelist/Challenger**：近战连段 + 格挡反击。用 `UGE_EnemyGuard` + `GuardDamageReduction`，格挡被破后 `OnGuardBroken` 进入失衡。需加格挡进入条件（玩家攻击时）。
- **机枪手 Suppressor/Enforcer**：远程压制 + 换位 + 区域封锁。`TickMobileGunner` 已落地，需加换位点 / 掩体点 / 最低安全距离，不能永久站桩。
- **天皇 Commander/Boss**：阶段、护盾、场地广播、支援调度。见步骤 7。

### 步骤 7：天皇三阶段

现状：`GetBossPhase` 只按血量算 1/2/3，**没有任何代码读它改行为**。

要做（按方案三阶段设计）：

- 阶段 0：广播守卫（天皇无敌/观察，看门狗先上，机枪手建压制，武士延迟加入）
- 阶段 1（100%–70%）：指挥与分工，扇形广播冲击 / 定点 AoE / 短防御，给支援强化，攻击后 0.8–1.2s 反击窗口
- 阶段 2（70%–35%）：战场广播，激活 2–3 个广播节点，天皇获护盾（由支援存活供能），机枪手换位，看门狗长扑击+扑空失衡，武士格挡反击，支援死亡后 Boss 减能力
- 阶段 3（35%–0%）：终战广播，取消常规护盾，每轮一个高威胁 Ability + 恢复，全场广播冲击，终结技（长预警+安全区+可打断），失败后 1.5s 大反击窗口，不再刷新支援

### 步骤 8：1945 场地

现状：`Content/_Slime/Days/08/0815/Y1945` 有敌人 BP（Emperor/Samurai/Gunner/Watchdog）、Watchdog 动画、Souvenir，**无 Director、无广播节点、无增援编排**。

要做：

- 放置 `AEnemyEncounterDirector`，配 `UEnemyEncounterDefinition`
- 建广播节点 Actor（周期性危险区，可破坏）
- 配增援顺序（阶段 0 看门狗 → 阶段 1 武士 → 阶段 2 广播节点 → 阶段 3 终结技）
- 完成奖励

资产策略：第一阶段复用现有网格/动画/Niagara/材质，只补攻击预警 Cue / 受击 Cue / 阶段切换 Cue / 广播危险区 Cue / 占位动画。不修改 Marketplace 原资产。

### 步骤 9：清理旧路径

- `AEnemyFighterAIController` 的逐帧状态机降级为兼容层（无 StateTree 资产时回退）
- `AEnemyTower` 的独立机枪逻辑迁入共享 Ability / StateTree，保留兼容入口
- `UEnemyCombatComponent` 保留为兼容门面，内部转 GAS

### 步骤 10：编译 + 验收

- 完整 UBT 编译无新增 UHT/C++ 错误
- 用户手动验收 1945：每阶段进入、Ability 触发、攻击权申请失败、硬直、死亡事件

## 4. 关键文件清单

### 本次新增

| 文件 | 用途 |
|------|------|
| `Source/SlimeFable/Enemy/SlimeEnemyGameplayTags.h/.cpp` | Native gameplay tags |
| `Source/SlimeFable/Enemy/EnemyGameplayEffects.h/.cpp` | C++ 构造的 GE 类 |

### 本次修改

| 文件 | 改动 |
|------|------|
| `Source/SlimeFable/Enemy/EnemyAttributeSet.h/.cpp` | 重写：8 真实属性 + 3 meta 属性 + Pre/Post 回调 |
| `Source/SlimeFable/Enemy/EnemyCharacter.h/.cpp` | GAS 权威伤害链路、失衡、格挡、门面同步、`InitAbilitySystem` |
| `Config/DefaultGameplayTags.ini` | 删除手写 enemy tag（改 native） |

### 既有（上一个 agent 留下，未本次改动）

| 文件 | 用途 |
|------|------|
| `Source/SlimeFable/Enemy/EnemyEncounterSubsystem.h/.cpp` | 最小版攻击权 + GetBossPhase |
| `Source/SlimeFable/Enemy/EnemyFighterAIController.cpp` | 已接 RequestAttackSlot |
| `Source/SlimeFable/Enemy/EnemyTower.h/.cpp` | 已加 MobileGunner |

### 待新增（下一步）

- `Source/SlimeFable/Enemy/Abilities/EnemySkillAbility.h/.cpp`（步骤 3）
- `Source/SlimeFable/Enemy/StateTree/STTask_*.h/.cpp`（步骤 4，7 个 Task）
- `Source/SlimeFable/Enemy/StateTree/STEvaluator_*.h/.cpp`（步骤 4，3 个 Evaluator）
- `Source/SlimeFable/Enemy/Encounter/EnemyEncounterDirector.h/.cpp`（步骤 5）
- `Source/SlimeFable/Enemy/Encounter/EnemyEncounterDefinition.h/.cpp`（步骤 5，数据资产）

## 5. 给下一个 agent 的接手指南

### 第一步：编译验证

先跑完整 UBT，修掉本次代码的编译错误。重点检查：

- `EnemyCharacter.cpp` 里 `UGE_EnemyDamage / UGE_EnemyStagger` 等前向声明是否齐全
- `SlimeEnemyGameplayTags.h` 的 `UE_DECLARE_GAMEPLAY_TAG_EXTERN` 宏在 5.8 的正确写法（参考引擎 `NativeGameplayTags.h`）
- `EnemyGameplayEffects.cpp` 里 `FSetByCallerFloat` 构造、`EGameplayModOp::MultiplyCompound` 枚举名
- `HandleHealthFacadeChanged` 是 `UFUNCTION` 动态委托，签名要和 `FOnSlimeHealthChanged` 完全一致（`float, float`）
- `TickPoiseRegen` 已写但**未在 Tick 里调用**，记得加

### 第二步：补步骤 1–2 剩余

- `Tick` 里调 `TickPoiseRegen`
- 死亡路径走 GAS（`OnGasDamageApplied` 检测 Health 归零 → `HandleDied`）

### 第三步：按步骤 3→4→5→6→7→8 推进

建议顺序：先做步骤 3（Ability 迁移）拿到可编译的中间态，再做步骤 4（StateTree）和步骤 5（Director），最后做 6/7/8（行为与编排）。每完成一步跑一次 UBT。

### 第四步：不要做的事

- 不要并行叠 MCP 调用
- 不要用 MCP 逐个建 366 关
- 不要改 `~/.cursor/skills-cursor/`
- 不要在 `Config/DefaultGameplayTags.ini` 里重新手写 `Combat.State.* / Combat.Event.* / Combat.Role.*`（已改 native）
- 不要主动写用户未要的 Markdown（本文件除外）
- 不要启动 PIE / 自动化玩法测试（用户手动验收）

### 当前 git 状态

工作区有未提交改动（本次 + 上一个 agent 的）。下一个 agent 接手时建议先 `git status` 看全貌。本次新增的 4 个文件（`SlimeEnemyGameplayTags.*` / `EnemyGameplayEffects.*`）是 untracked。
