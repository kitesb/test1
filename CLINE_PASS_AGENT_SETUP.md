# Cline Pass 接入 OpenAI 兼容 API —— 指南 + 自验证清单（给 Codex 执行）

> 本文档自包含：先给结论，再给一份可逐步执行的**自验证清单**（每步有命令、期望输出、失败对策），最后附配置模板、模型表、排错表。
> Codex 应按第 2 节顺序执行；每步通过再进入下一步。

---

## 0. 结论速览

| 项 | 值 |
|---|---|
| Base URL | `https://api.cline.bot/api/v1` |
| 端点 | `POST /chat/completions`（OpenAI Chat Completions 兼容） |
| 鉴权 | `Authorization: Bearer $CLINE_API_KEY` |
| Key 来源 | https://app.cline.bot → Settings → API Keys |
| 模型字段 | 完整 slug，例如 `cline-pass/qwen3.7-max` |
| 计费 | `cline-pass/*` 走订阅（token plan），**不**按量扣 credits；`anthropic/*`、`openai/*` 等按量付费 |
| 协议特性 | SSE 流式 ✅、tools/function calling ✅、reasoning（`delta.reasoning`）✅、图片输入 ✅ |

### 两个必须先知道的坑

1. **Codex 不能直连 Cline。** 新版 Codex 只支持 Responses API（`/v1/responses`），且已**硬删** `wire_api = "chat"`；Cline 只有 `/chat/completions`，`/v1/responses` 返回 `404`。
   → 解决：用 **LiteLLM** 把 Responses 桥接成 Chat（第 2 节 Step 4/5），或降级到 2026-02 之前的 Codex 直连。
2. **只用 `cline-pass/*` 模型**，用 `anthropic/*` 之类会扣 credits；`cline-free/*` 外部 API 直接 403。

---

## 1. 关键事实（已实测，供判断用）

- `GET https://api.cline.bot/api/v1/models` 返回 458 个模型，但**不包含 `cline-pass/*`**（公开列表过滤掉了订阅模型）。所以不能用它发现 `cline-pass` 模型。
- `POST /api/v1/chat/completions` 流式返回标准 OpenAI SSE：`data: {"object":"chat.completion.chunk",...}`，结尾 `data: [DONE]`。
- `POST /api/v1/chat/completions` 非流式，用 Cline OAuth/沙箱身份时会包一层：`{"success":true,"data":{"choices":[...]}}`。
- `POST /api/v1/responses`、`/responses`、`/v1/responses` 三个路径均返回 `404 {"error":"Not Found"}` → Cline 无 Responses API。
- `cline-free/*` 模型经外部 API 调用返回 `403 ... only available via Cline product surfaces`。
- 新版 Codex 对 `wire_api = "chat"` 直接报错：`` `wire_api = "chat"` is no longer supported. ``（只接受 `"responses"`）。

---

## 2. 自验证清单（Codex 按顺序执行）

> 约定：`$BASE=https://api.cline.bot/api/v1`，`$KEY` = Cline API Key（沙箱内可留空，见 Step 0）。
> 每一步都有「期望」和「失败对策」。**不要跳过 Step 3**，它决定了你是否需要做桥接。

### Step 0 — 环境与凭证

```bash
export BASE="https://api.cline.bot/api/v1"
export MODEL="cline-pass/mimo-v2.6-flash"   # 先用便宜快的做冒烟
export KEY="${CLINE_API_KEY:-}"             # 有真实 key 就设；沙箱内可留空

echo "BASE=$BASE"; echo "KEY set? $([ -n "$KEY" ] && echo yes || echo no)"
command -v jq >/dev/null && echo "jq: ok" || echo "jq: 缺失（可用 python3 代替）"
command -v python3 >/dev/null && echo "python3: ok" || echo "python3: 缺失"
```

**期望**：`BASE` 正确；`python3: ok`。

### Step 1 — 冒烟：Base URL + 订阅模型（流式）

```bash
curl -sS -N -X POST "$BASE/chat/completions" \
  ${KEY:+-H "Authorization: Bearer $KEY"} \
  -H 'Content-Type: application/json' \
  -d "{\"model\":\"$MODEL\",\"messages\":[{\"role\":\"user\",\"content\":\"只回复两个字：可以\"}],\"stream\":true,\"max_tokens\":64}" \
  | head -c 600
```

**期望**：出现若干行 `data: {..."object":"chat.completion.chunk"...}`，其中有 `"content"` 或 `"reasoning"` 字段。
**通过判定**：能看到 `chat.completion.chunk` 即 base url 正确、鉴权通过、模型可用。
**失败对策**：
- `401 Unauthorized` → `$KEY` 不对或未带；检查 Key。
- `402 / insufficient_credits` → 说明你用了按量付费模型；确认 `$MODEL` 以 `cline-pass/` 开头。
- `403 ... product surfaces` → 用了 `cline-free/*`；换成 `cline-pass/*`。
- 连接失败 → 网络/代理问题，确认能访问 `api.cline.bot`。

### Step 2 — 非流式 + 响应包裹处理

```bash
RESP=$(curl -sS -X POST "$BASE/chat/completions" \
  ${KEY:+-H "Authorization: Bearer $KEY"} \
  -H 'Content-Type: application/json' \
  -d "{\"model\":\"$MODEL\",\"messages\":[{\"role\":\"user\",\"content\":\"只回复两个字：可以\"}],\"max_tokens\":64}")

echo "$RESP" | python3 -c '
import json,sys
d=json.load(sys.stdin)
body=d.get("data",d) if isinstance(d,dict) else d
if isinstance(body,dict) and body.get("choices"):
    print("content:", (body["choices"][0]["message"].get("content") or "").strip())
    print("wrapped:", "data" in d)
else:
    print("NO_CHOICES", str(sys.stdin)[:200])
'
```

**期望**：打印 `content: 可以`，并可能 `wrapped: True`。
**通过判定**：能取到 `content`。
**注意**：若 `wrapped: True`，说明非流式被包了 `data`；普通 OpenAI 客户端需解包，或**改用流式**（Codex 本身走流式，不受影响）。

### Step 3 — 确认 Cline 无 Responses API（决定是否必须桥接）

```bash
for p in /api/v1/responses /responses /v1/responses; do
  code=$(curl -s -o /dev/null -w '%{http_code}' -X POST "https://api.cline.bot${p}" \
    ${KEY:+-H "Authorization: Bearer $KEY"} -H 'Content-Type: application/json' \
    -d '{"model":"cline-pass/mimo-v2.6-flash","input":"hi"}')
  echo "${p} -> HTTP ${code}"
done
```

**期望**：三个都 `HTTP 404`。
**通过判定**：确认 Cline 无 Responses 端点。
**结论**：
- 若你的 Codex **仍支持** `wire_api = "chat"` → 走 **Step 5B（直连）**。
- 若你的 Codex **只管** `wire_api = "responses"` → 必须走 **Step 4 + Step 5A（LiteLLM 桥接）**。

### Step 4 — 搭 LiteLLM 桥接并验证 `/v1/responses`

**4.1 安装**

```bash
pip install "litellm[proxy]" || uv tool install "litellm[proxy]"
litellm --version
```

**4.2 写配置 `litellm-config.yaml`**

```yaml
model_list:
  - model_name: "cline-pass/qwen3.7-max"
    litellm_params:
      model: "openai/cline-pass/qwen3.7-max"     # openai/ 前缀 = 走 OpenAI 兼容 chat/completions
      api_base: "https://api.cline.bot/api/v1"
      api_key: "os.environ/CLINE_API_KEY"
    model_info:
      mode: chat                                  # 关键：促使 /v1/responses 自动桥接到 chat
  - model_name: "cline-pass/mimo-v2.6-flash"
    litellm_params:
      model: "openai/cline-pass/mimo-v2.6-flash"
      api_base: "https://api.cline.bot/api/v1"
      api_key: "os.environ/CLINE_API_KEY"
    model_info:
      mode: chat
general_settings:
  master_key: "sk-local-test"
```

**4.3 启动（后台）**

```bash
export CLINE_API_KEY="${KEY:-dummy}"          # 沙箱内随便填也行（代理会覆盖）
litellm --config litellm-config.yaml --port 4000 >/tmp/litellm.log 2>&1 &
echo $! > /tmp/litellm.pid
sleep 5
curl -s -o /dev/null -w 'litellm health: %{http_code}\n' http://localhost:4000/health/liveliness || true
```

**4.4 验证桥接（关键一步）**

```bash
curl -sS -X POST http://localhost:4000/v1/responses \
  -H "Authorization: Bearer sk-local-test" \
  -H 'Content-Type: application/json' \
  -d '{"model":"cline-pass/mimo-v2.6-flash","input":"say pong","max_output_tokens":32}' \
  | head -c 800
```

**期望**：返回 Responses 结构 `{"object":"response","output":[...],"status":"completed",...}`，`output` 里能找到文本。
**通过判定**：拿到 `"object":"response"` 且有内容 → 桥接可用。
**失败对策**：
- `model not found` → `model_name` 与请求里的 `model` 必须**完全一致**。
- 返回的是 chat 结构或报错 → 确认 `model_info.mode: chat` 存在；看 `/tmp/litellm.log`。
- 上游 401/402 → 回到 Step 1 检查 `$KEY` 与模型。

### Step 5A — 配置并运行 Codex（经 LiteLLM，任意 Codex 版本）

写入 `~/.codex/config.toml`：

```toml
model = "cline-pass/mimo-v2.6-flash"
model_provider = "local-litellm"

[model_providers.local-litellm]
name = "ClinePass via LiteLLM"
base_url = "http://localhost:4000/v1"
wire_api = "responses"
env_key = "LITELLM_KEY"
```

```bash
export LITELLM_KEY="sk-local-test"
codex exec --skip-git-repo-check "reply with exactly: cline-pass-ok" 2>&1 | tail -30
```

**期望**：Codex 输出里出现 `cline-pass-ok`（模型确实回了话）。
**通过判定**：端到端通。
**失败对策**：
- Codex 仍想用 OpenAI/要登录 → 确认 `model_provider` 拼写与 `[model_providers.xxx]` 一致；必要时加 `--model-provider local-litellm`（视版本而定）。
- 报 `wire_api` 相关错 → 该版本可能不接受某些字段；确保只有 `wire_api = "responses"`。
- 连不上 4000 → 确认 Step 4.3 的 litellm 进程还在（`cat /tmp/litellm.pid`）。

### Step 5B — 备选：旧版 Codex 直连（仅当 Step 3 判定支持 chat）

```bash
npm i -g @openai/codex@0.87.0   # 2026-01-16 发布，确定支持 wire_api="chat"
codex --version
```

`~/.codex/config.toml`：

```toml
model = "cline-pass/qwen3.7-max"
model_provider = "cline-pass"

[model_providers.cline-pass]
name = "ClinePass"
base_url = "https://api.cline.bot/api/v1"
env_key = "CLINE_API_KEY"
wire_api = "chat"
```

```bash
export CLINE_API_KEY="你的key"
codex exec --skip-git-repo-check "reply with exactly: cline-pass-ok" 2>&1 | tail -30
```

**期望**：输出包含 `cline-pass-ok`。

### Step 6 — 收尾

```bash
[ -f /tmp/litellm.pid ] && kill "$(cat /tmp/litellm.pid)" 2>/dev/null; echo cleaned
```

**最终判定**：Step 1 通过 = API/Base URL 可用；Step 4.4 通过 = 桥接可用；Step 5A/5B 通过 = Codex 端到端可用。

---

## 3. 配置参考（可直接复制）

### 3.1 LiteLLM `litellm-config.yaml`

见 Step 4.2。要点：`model: openai/<slug>`、`api_base` 指向 Cline、`model_info.mode: chat`。

### 3.2 Codex `~/.codex/config.toml`（经 LiteLLM）

```toml
model = "cline-pass/qwen3.7-max"
model_provider = "local-litellm"

[model_providers.local-litellm]
name = "ClinePass via LiteLLM"
base_url = "http://localhost:4000/v1"
wire_api = "responses"
env_key = "LITELLM_KEY"
```

### 3.3 直接 curl（任何工具都能用）

```bash
export CLINE_API_KEY="你的key"
curl -X POST https://api.cline.bot/api/v1/chat/completions \
  -H "Authorization: Bearer $CLINE_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{"model":"cline-pass/qwen3.7-max","messages":[{"role":"user","content":"hello"}],"stream":false}'
```

### 3.4 Python（OpenAI SDK）

```python
from openai import OpenAI
client = OpenAI(base_url="https://api.cline.bot/api/v1", api_key="YOUR_API_KEY")
# 建议 stream=True，避免非流式被包 data
stream = client.chat.completions.create(
    model="cline-pass/qwen3.7-max",
    messages=[{"role": "user", "content": "hello"}],
    stream=True,
)
for chunk in stream:
    print(chunk.choices[0].delta.content or "", end="")
```

---

## 4. Cline Pass 模型 slug 表

| 模型 | slug |
|---|---|
| GLM-5.3 | `cline-pass/glm-5.3` |
| GLM-5.3 Flash | `cline-pass/glm-5.3-flash` |
| Kimi K3 | `cline-pass/kimi-k3` |
| DeepSeek V4 Pro | `cline-pass/deepseek-v4-pro` |
| DeepSeek V4.1 Flash | `cline-pass/deepseek-v4.1-flash` |
| MiMo-V2.5 | `cline-pass/mimo-v2.5` |
| MiMo-V2.5-Pro | `cline-pass/mimo-v2.5-pro` |
| MiniMax M3 | `cline-pass/minimax-m3` |
| Muse Spark 1.3 Contributor | `cline-pass/muse-spark-1.3-contributor` |
| Qwen3.8 Max | `cline-pass/qwen3.8-max` |
| Qwen3.7 Max | `cline-pass/qwen3.7-max` |
| Qwen3.7 Plus | `cline-pass/qwen3.7-plus` |

> 额外在沙箱 SDK 里见过：`cline-pass/mimo-v2.6-flash`、`cline-pass/mimo-v2.6-pro`（可用，稳定性不保证）。
> `GET /models` **不会**列出以上任何 `cline-pass/*`，请照表使用。

---

## 5. 排错表

| 现象 | 原因 / 解决 |
|---|---|
| Codex: `wire_api = "chat" is no longer supported` | Codex 太新。改用 Step 4+5A（LiteLLM），或降级到 2026-02 前的版本 |
| Codex 连 Cline 报 404 | Codex 打 `/v1/responses`，Cline 没有 → 必须桥接或旧版 Codex |
| `402` / `insufficient_credits` | 用了按量付费模型 → 改用 `cline-pass/*` |
| `403 ... only available via Cline product surfaces` | 用了 `cline-free/*` → 换 `cline-pass/*` |
| 非流式读不到 `choices` | 被包成 `{success,data}` → 改流式或解包 `.data` |
| `401 Unauthorized` | Key 错误/缺失 → `Authorization: Bearer <key>` |
| `litellm /v1/responses` 报 model not found | `model_name` 与 Codex 的 `model` 不一致；或缺 `model_info.mode: chat` |
| Codex 忽略自定义 provider、仍要 OpenAI 登录 | `model_provider` 名称与 `[model_providers.<name>]` 不一致；或需 `--model-provider <name>` |

---

## 6. 沙箱环境 vs 生产环境

- **本 Cline 沙箱内**：egress 代理会自动给发往 `api.cline.bot` 的请求注入 Cline 凭证，**显式 `Authorization` 也会被覆盖**。因此沙箱内不填 key 也能通过 Step 1/2（且是订阅身份，不扣 credits）。仅用于本地验证。
- **沙箱外 / 你自己机器**：**必须**在 https://app.cline.bot → Settings → API Keys 创建真实 Key，并作为 `CLINE_API_KEY` 传入。

---

## 7. 其它 Agent（说 Chat Completions 的，直接接）

### Aider
```bash
export OPENAI_API_BASE="https://api.cline.bot/api/v1"
export OPENAI_API_KEY="你的key"
aider --model openai/cline-pass/qwen3.7-max
```

### Continue（config.json）
```json
{"models":[{"title":"ClinePass Qwen3.7 Max","provider":"openai","model":"cline-pass/qwen3.7-max","apiBase":"https://api.cline.bot/api/v1","apiKey":"你的key"}]}
```

### Cline 自己 / CLI（原生支持）
```bash
cline auth -p cline-pass
```

### 自写脚本
用标准 OpenAI 客户端换 `base_url` 即可（见 3.4）。
