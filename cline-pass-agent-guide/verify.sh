#!/usr/bin/env bash
# 自测 Cline Pass 作为 OpenAI 兼容 API 是否可用。
#
# 用法：
#   CLINE_API_KEY=xxx ./verify.sh                 # 用真实 key（沙箱外必须）
#   ./verify.sh                                    # 沙箱内可不带 key（代理注入）
#
# 只会调用 cline-pass/* 订阅模型，不会扣 credits。
set -euo pipefail

BASE_URL="${CLINE_BASE_URL:-https://api.cline.bot/api/v1}"
MODEL="${CLINE_MODEL:-cline-pass/qwen3.7-max}"
KEY="${CLINE_API_KEY:-}"   # 沙箱内留空即可

echo "== base : $BASE_URL"
echo "== model: $MODEL"

if [[ -n "$KEY" ]]; then
  AUTH_HEADER="Authorization: Bearer $KEY"
  echo "== auth : 使用 CLINE_API_KEY"
else
  AUTH_HEADER=""
  echo "== auth : 未提供 key（沙箱内由 egress 代理注入）"
fi

echo
echo "== 1) 流式请求（标准 OpenAI SSE，最可靠）=="
curl -sS -N -X POST "$BASE_URL/chat/completions" \
  ${AUTH_HEADER:+-H "$AUTH_HEADER"} \
  -H 'Content-Type: application/json' \
  -d "{\"model\":\"$MODEL\",\"messages\":[{\"role\":\"user\",\"content\":\"只回复两个字：可以\"}],\"stream\":true,\"max_tokens\":64}" \
  | grep -m1 '"content"' | head -c 400 2>/dev/null || true

echo
echo
echo "== 2) 非流式请求（注意可能被包成 {success,data}）=="
RESP=$(curl -sS -X POST "$BASE_URL/chat/completions" \
  ${AUTH_HEADER:+-H "$AUTH_HEADER"} \
  -H 'Content-Type: application/json' \
  -d "{\"model\":\"$MODEL\",\"messages\":[{\"role\":\"user\",\"content\":\"只回复两个字：可以\"}],\"max_tokens\":64}")

python3 - "$RESP" <<'PY' || echo "$RESP" | head -c 600
import json, sys
raw = sys.argv[1]
try:
    d = json.loads(raw)
except Exception:
    print("非 JSON 响应:", raw[:300]); sys.exit(0)
# 兼容两种结构
body = d.get("data", d) if isinstance(d, dict) else d
if isinstance(body, dict) and body.get("choices"):
    msg = body["choices"][0].get("message", {})
    print("OK  content:", (msg.get("content") or "").strip()[:200])
    print("   （注意：非流式被包了一层 data 时，OpenAI 客户端需解包 .data）" if "data" in d else "")
else:
    print("未拿到 choices。原始响应：")
    print(raw[:600])
PY

echo
echo "== 完成。若上面能看到模型回复即表示 base url + 模型可用。=="
