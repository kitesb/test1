# 把 Cline Pass 当作 OpenAI 兼容 API / Base URL 给其它 Agent 使用

> 验证环境：Cline 沙箱，2026-10-10。下面每条关键结论都做了实测或查了官方源码/文档。
> 目标读者：想让 Codex / Aider / Continue / 自写脚本 等"别的 agent"复用 Cline Pass 的人。

---

## 0. 30 秒速览

| 项目 | 值 |
|---|---|
| Base URL | `https://api.cline.bot/api/v1` |
| 端点 | `POST /chat/completions`（OpenAI Chat Completions 兼容） |
| 鉴权 | `Authorization: Bearer $CLINE_API_KEY` |
| API Key 来源 | https://app.cline.bot → **Settings → API Keys** |
| 模型字段 | 完整 slug，例如 `cline-pass/qwen3.7-max` |
| 计费 | `cline-pass/*` 走订阅（token plan），**不**按量扣 credits；`anthropic/*`、`openai/*` 等走按量付费 |

支持的协议特性（实测）：SSE 流式、`tools`/function calling、`reasoning`（思考内容在 `delta.reasoning`）、图片输入。

### ⚠️ 两个必须先知道的坑

1. **Codex 只认 Responses API，Cline 只有 Chat Completions。**
   新版 Codex CLI 已**删除** `wire_api = "chat"`（2026-02 起变硬报错），只支持 `wire_api = "responses"`（打到 `/v1/responses`）。
   而 Cline **没有** `/v1/responses`（实测三个路径都返回 `404 {"error":"Not Found"}`）。
   👉 所以 Codex 不能直接连 Cline，必须二选一：**旧版 Codex 直连**（第 2 节方案 B）或 **LiteLLM 桥接**（第 2 节方案 A，推荐）。

2. **别用按量付费模型，否则扣 credits。**
   模型 slug 必须以 `cline-pass/` 开头才是订阅内。用 `anthropic/claude-...`、`openai/gpt-...` 之类会走按量付费，余额不足就报 `insufficient_credits`。

---

## 1. 通用用法（任何 OpenAI 兼容工具）

### 1.1 直接 curl

```bash
export CLINE_API_KEY="你的_api_key"

curl -X POST https://api.cline.bot/api/v1/chat/completions \
  -H "Authorization: Bearer $CLINE_API_KEY" \
  -H "Content-Type: application/json" \
  -d '{
    "model": "cline-pass/qwen3.7-max",
    "messages": [{"role": "user", "content": "用一句话解释递归"}],
    "stream": false
  }'
```

### 1.2 Python（OpenAI SDK）

```python
from openai import OpenAI

client = OpenAI(
    base_url="https://api.cline.bot/api/v1",
    api_key="YOUR_API_KEY",
)

resp = client.chat.completions.create(
    model="cline-pass/qwen3.7-max",
    messages=[{"role": "user", "content": "hello"}],
)
print(resp.choices[0].message.content)
```

### 1.3 Node.js（OpenAI SDK）

```ts
import OpenAI from "openai";

const client = new OpenAI({
  baseURL: "https://api.cline.bot/api/v1",
  apiKey: process.env.CLINE_API_KEY,
});

const resp = await client.chat.completions.create({
  model: "cline-pass/qwen3.7-max",
  messages: [{ role: "user", content: "hello" }],
});
console.log(resp.choices[0].message.content);
```

### 1.4 响应结构注意事项（重要）

- **流式（`stream: true`）**：标准 OpenAI SSE，`data: {...}` 一行一条，结尾 `data: [DONE]`。任何 OpenAI 客户端都能直接消费。
- **非流式**：如果用的是 Cline 的 OAuth/沙箱身份，返回会被包一层：
  ```json
  { "success": true, "data": { "choices": [ ... ] } }
  ```
  标准 OpenAI 客户端会读不到 `choices`。处理办法：**改用流式**，或读 `resp["data"]`。
  （用官方 API Key 时通常直接是标准结构；遇到包裹时按上述处理即可。）

### 1.5 Cline Pass 模型 slug 一览

来自官方文档 `docs.cline.bot/getting-started/clinepass`：

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

> 备注：`GET /api/v1/models` **不会**列出 `cline-pass/*`（公开列表里被过滤掉了），所以别指望用它发现模型，按上表用即可。
> 沙箱内 SDK 里还见过 `cline-pass/mimo-v2.6-flash`、`cline-pass/mimo-v2.6-pro`，可用但不保证长期存在。

---

## 2. 让 Codex 用上 Cline Pass

### 方案 A（推荐，任意 Codex 版本都行）：用 LiteLLM 把 Responses 桥接成 Chat

原理：Codex 只发 `/v1/responses`，LiteLLM 的 `/v1/responses` 端点会在上游 provider "只有 chat/completions" 时自动桥接（官方文档：`/responses` — "Requests ... may be bridged ... when the provider lacks support"）。于是：

```
Codex  ──(Responses API)──▶  LiteLLM :4000  ──(Chat Completions)──▶  api.cline.bot/api/v1
```

**① 安装 LiteLLM**

```bash
pip install "litellm[proxy]"
# 或 uv: uv tool install "litellm[proxy]"
```

**② 写 `litellm-config.yaml`**（本仓库已附同名文件）

```yaml
model_list:
  - model_name: "cline-pass/qwen3.7-max"
    litellm_params:
      model: "openai/cline-pass/qwen3.7-max"   # openai/ 前缀 = 走 OpenAI 兼容的 chat/completions
      api_base: "https://api.cline.bot/api/v1"
      api_key: "os.environ/CLINE_API_KEY"
    model_info:
      mode: chat                                # 关键：声明这是 chat 模型，促使 /responses 走桥接

  - model_name: "cline-pass/kimi-k3"
    litellm_params:
      model: "openai/cline-pass/kimi-k3"
      api_base: "https://api.cline.bot/api/v1"
      api_key: "os.environ/CLINE_API_KEY"
    model_info:
      mode: chat

general_settings:
  master_key: "sk-anything-local"               # 本地随便设，Codex 用它当 api key
```

**③ 启动 LiteLLM**

```bash
export CLINE_API_KEY="你的_cline_api_key"
litellm --config litellm-config.yaml --port 4000
```

先用 curl 自测桥接是否通（这步很关键，先确认 `/v1/responses` 真能出结果）：

```bash
curl -X POST http://localhost:4000/v1/responses \
  -H "Authorization: Bearer sk-anything-local" \
  -H "Content-Type: application/json" \
  -d '{"model":"cline-pass/qwen3.7-max","input":"say pong"}'
```

**④ 配置 Codex：`~/.codex/config.toml`**

```toml
model = "cline-pass/qwen3.7-max"
model_provider = "local-litellm"

[model_providers.local-litellm]
name = "ClinePass via LiteLLM"
base_url = "http://localhost:4000/v1"
wire_api = "responses"
env_key = "LITELLM_KEY"
```

**⑤ 跑起来**

```bash
export LITELLM_KEY="sk-anything-local"
codex
```

> 如果 Codex 提示需要登录/找 OpenAI key，可加 `--model-provider local-litellm` 或确认 config 里 `model_provider` 拼写一致。

---

### 方案 B：用旧版 Codex 直连（Chat wire，最省事但需降级）

只有 **2026-02 之前** 的 Codex 版本还接受 `wire_api = "chat"`。之后的版本对它直接硬报错：
`\`wire_api = "chat"\` is no longer supported.`

**① 装一个旧版（示例：0.87.0，2026-01-16，确定支持 chat）**

```bash
npm i -g @openai/codex@0.87.0
codex --version
```

**② `~/.codex/config.toml`**

```toml
model = "cline-pass/qwen3.7-max"
model_provider = "cline-pass"

[model_providers.cline-pass]
name = "ClinePass"
base_url = "https://api.cline.bot/api/v1"
env_key = "CLINE_API_KEY"
wire_api = "chat"
```

**③ 运行**

```bash
export CLINE_API_KEY="你的_cline_api_key"
codex
```

> 直连时注意 1.4 的非流式包裹问题——Codex 本身走流式，一般不受影响。

---

## 3. 其它 Agent 怎么接

大多数编码 agent 说的是 Chat Completions，直接填 **Base URL + API Key + model=cline-pass/xxx** 即可。

### Aider

```bash
export OPENAI_API_BASE="https://api.cline.bot/api/v1"
export OPENAI_API_KEY="你的_cline_api_key"
aider --model openai/cline-pass/qwen3.7-max
```

### Continue（`config.json`）

```json
{
  "models": [{
    "title": "ClinePass Qwen3.7 Max",
    "provider": "openai",
    "model": "cline-pass/qwen3.7-max",
    "apiBase": "https://api.cline.bot/api/v1",
    "apiKey": "你的_cline_api_key"
  }]
}
```

### Cline 自己 / CLI

Cline 的 IDE 扩展和 CLI **原生**支持 ClinePass，直接选 provider = **ClinePass** 即可，不用折腾 API。

```bash
cline auth -p cline-pass
```

### 任何自写脚本

就是标准 OpenAI 客户端换 `base_url`，见 1.2 / 1.3。

---

## 4. 排错速查

| 现象 | 原因 / 解决 |
|---|---|
| Codex 报 `wire_api = "chat" is no longer supported` | 你的 Codex 太新。走方案 A（LiteLLM），或用方案 B 降级到 2026-02 前的版本 |
| Codex 连 Cline 报 404 | Codex 打的是 `/v1/responses`，Cline 没有。必须用桥接或旧版 Codex |
| `402` / `insufficient_credits` | 用了按量付费模型（`anthropic/*` 等）。**改用 `cline-pass/*`** |
| `403 ... only available via Cline product surfaces` | 用了 `cline-free/*`，它只在 Cline 官方界面里可用，外部 API 不给 |
| 非流式返回读不到 `choices` | 被包成 `{success,data}`，见 1.4：改流式或解包 `.data` |
| `401 Unauthorized` | API Key 不对/没带。`Authorization: Bearer <key>` |
| 想列出模型却没有 cline-pass | `/models` 不列 `cline-pass/*`，按 1.5 的表用 |
| LiteLLM `/v1/responses` 报 model not found | 确认 `model_name` 与 Codex 里的 `model` 完全一致；确认 `model_info.mode: chat` |

---

## 5. 在「本沙箱」里的最快验证（无需任何 key）

Cline 沙箱里有个 egress 代理，会自动给发往 `api.cline.bot` 的请求注入 Cline 凭证，**显式传的 Authorization 也会被覆盖**。所以沙箱里任何 agent 直接打 `https://api.cline.bot/api/v1` 就能用，key 随便填。

```bash
# 沙箱内：不传 key 也能通（且是订阅身份，不会扣 credits）
curl -s -X POST https://api.cline.bot/api/v1/chat/completions \
  -H 'Content-Type: application/json' \
  -d '{"model":"cline-pass/mimo-v2.6-flash","messages":[{"role":"user","content":"say pong"}],"max_tokens":16}'
```

> 仅限本沙箱。出了沙箱（比如你自己机器上的 Codex），**必须**用真实的 `CLINE_API_KEY`。
> 另外无论在哪，`cline-free/*` 都只能通过 Cline 官方界面使用，外部 API 调用会返回 403；请用 `cline-pass/*`。

---

## 附：本目录文件

- `litellm-config.yaml` — 方案 A 的 LiteLLM 配置示例
- `codex-config.toml` — Codex 配置示例（含 A/B 两套）
- `verify.sh` — 一条命令自测 base url + key + 模型是否可用
