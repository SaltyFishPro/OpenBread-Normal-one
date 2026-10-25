"""Generate the standalone XMB preview from the current firmware icon export."""

import json
import re
from pathlib import Path


HERE = Path(__file__).resolve().parent
SOURCE = HERE / "main_menu_mock.html"
OUTPUT = HERE / "xmb_menu_mock.html"


TEMPLATE = r'''<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>OpenBread XMB 主菜单方案</title>
<style>
  :root { --s: 2; --x: 0px; --y: 0px; }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { background: #e8e8e8; color: #111; font: 14px/1.5 "Segoe UI", "Microsoft YaHei", sans-serif;
    display: flex; flex-direction: column; align-items: center; gap: 14px; padding: 28px 16px 40px; }
  button { font: inherit; cursor: pointer; }
  .topbar { display: flex; gap: 10px; flex-wrap: wrap; justify-content: center; }
  .topbar button, .keys button, .preview-tools button { border: 1px solid #111; background: #fff; padding: 8px 16px; }
  .topbar button.active { background: #111; color: #fff; }
  .stage { zoom: var(--s); outline: 1px solid #999; background: #fff; }
  .screen { --sleep-shift: 0px; --sleep-reveal: 0%; --sleep-line-x: 0px; width: 384px; height: 168px; position: relative; overflow: hidden; background: #000;
    font-family: "Microsoft YaHei", sans-serif; font-size: 14px; -webkit-font-smoothing: none; }
  .screen, .screen * { color: #fff; }
  [hidden] { display: none !important; }

  /* XMB：横轴是分类，纵轴是当前分类内项目。所有位移都以屏幕像素计算。 */
  .xmb-content { position: absolute; inset: 0; z-index: 1; transform: translateX(var(--sleep-shift)); }
  .top-status { position: absolute; z-index: 2; left: 12px; right: 12px; top: 3px; height: 24px; display: flex; align-items: center; justify-content: space-between; }
  .top-status .battery { font-size: 14px; line-height: 18px; }
  .top-status .clock { position: absolute; left: 50%; transform: translateX(-50%); font-family: Consolas, monospace; font-size: 14px; line-height: 18px; letter-spacing: 1px; }
  .top-status .date { font-size: 14px; line-height: 18px; }
  .wave { position: absolute; z-index: 0; left: -36px; bottom: -30px; width: 456px; height: 82px; border: 1px solid rgba(255,255,255,.44); border-color: rgba(255,255,255,.44) transparent transparent; border-radius: 50%; transform: rotate(-4deg); }
  .wave::after { content: ""; position: absolute; left: 8px; top: 11px; width: 438px; height: 70px; border: 1px solid rgba(255,255,255,.28); border-color: rgba(255,255,255,.28) transparent transparent; border-radius: 50%; }
  .xmb-axis { position: absolute; z-index: 1; inset: 0; transform: translate(var(--x), var(--y)); transition: transform .24s cubic-bezier(.2,.7,.3,1); }
  .category-rail { position: absolute; left: 0; top: 29px; width: 100%; height: 57px; display: flex; align-items: center; }
  .category-rail.category-enter-from-right { animation: categoryEnterFromRight .18s ease-out both; }
  .category-rail.category-enter-from-left { animation: categoryEnterFromLeft .18s ease-out both; }
  .category { position: absolute; top: 0; width: 58px; height: 52px; display: flex; flex-direction: column;
    align-items: center; gap: 2px; transition: transform .24s, opacity .24s; }
  .category canvas { width: 30px; height: 30px; image-rendering: pixelated; }
  .category.focus { width: 76px; height: 57px; top: -3px; gap: 1px; }
  .category.focus canvas { width: 42px; height: 42px; }
  .category.focus .category-label { font-size: 14px; }
  .category:not(.focus) { opacity: .55; }
  .category:not(.focus) .category-label { visibility: hidden; }
  .category-label { font-size: 14px; line-height: 18px; white-space: nowrap; }
  .category.focus .category-label::after { content: ""; display: block; height: 1px; margin: 1px 10px 0; background: #fff; }
  .rail-rule { position: absolute; left: 14px; right: 14px; top: 87px; border-top: 1px solid rgba(255,255,255,.45); }

  .section-panel { position: absolute; left: 0; top: 91px; width: 384px; height: 77px; }
  .item-list { position: absolute; left: 24px; top: 0; width: 348px; height: 77px; overflow: hidden; }
  .item-list.item-enter-from-bottom { animation: itemEnterFromBottom .15s ease-out both; }
  .item-list.item-enter-from-top { animation: itemEnterFromTop .15s ease-out both; }
  .item { position: absolute; left: 0; width: 348px; height: 28px; display: flex; align-items: center; gap: 9px;
    padding: 0 8px; transition: transform .2s, opacity .2s; }
  .item.focus { height: 28px; }
  .item-index { width: 24px; flex: 0 0 24px; font-family: Consolas, monospace; font-size: 14px; line-height: 18px; text-align: right; opacity: .68; }
  .item.focus .item-index { opacity: 1; }
  .item-name { font-size: 14px; line-height: 20px; white-space: nowrap; opacity: .78; }
  .item.focus .item-name { font-size: 14px; line-height: 20px; opacity: 1; }
  .item-rule { display: none; }
  .empty-items { font-size: 14px; position: absolute; left: 24px; top: 7px; opacity: .65; }
  .hint { max-width: 820px; color: #555; text-align: center; font-size: 14px; line-height: 1.7; }
  .keys { display: flex; gap: 10px; width: min(100%, 416px); }
  .keys button { flex: 1; min-width: 0; }
  .preview-tools { display: flex; gap: 12px; align-items: center; flex-wrap: wrap; justify-content: center; font-size: 14px; }
  .preview-tools button:disabled { cursor: default; opacity: .5; }
  .note { max-width: 820px; color: #555; text-align: center; font-size: 14px; line-height: 1.7; }
  .sleep-layer { position: absolute; z-index: 4; inset: 0; background: #000; color: #fff; clip-path: inset(0 calc(100% - var(--sleep-reveal)) 0 0); }
  .sleep-layer, .sleep-layer * { color: #fff; }
  .sleep-clock-col { position: absolute; left: 256px; top: 0; width: 128px; height: 168px; }
  .sleep-time-card { position: absolute; left: 8px; top: 16px; width: 112px; height: 44px; border: 2px solid #fff; border-radius: 8px; display: flex; align-items: center; justify-content: center; }
  .sleep-time-text { font-family: Consolas, monospace; font-size: 24px; line-height: 28px; font-variant-numeric: tabular-nums; }
  .sleep-date-card { position: absolute; left: 8px; top: 72px; width: 112px; height: 64px; border: 2px solid #fff; border-radius: 8px; display: grid; grid-template-columns: 1fr 1fr; grid-template-rows: 28px 18px; gap: 4px 8px; padding: 5px 8px; align-items: center; }
  .sleep-date-day { font-family: Consolas, monospace; font-size: 24px; line-height: 28px; }
  .sleep-date-week { background: #fff; color: #000 !important; font-size: 14px; line-height: 22px; text-align: center; }
  .sleep-date-month { grid-column: 2; grid-row: 2; font-size: 14px; line-height: 18px; text-align: center; }
  .sleep-date-year { grid-column: 1; grid-row: 2; font-size: 14px; line-height: 18px; }
  .sleep-battery { position: absolute; left: 16px; top: 146px; height: 18px; display: flex; gap: 8px; align-items: center; }
  .sleep-batt-body { width: 24px; height: 12px; border: 1px solid #fff; position: relative; }
  .sleep-batt-body::after { content: ""; position: absolute; right: -4px; top: 3px; width: 3px; height: 5px; background: #fff; }
  .sleep-batt-fill { position: absolute; left: 2px; top: 2px; bottom: 2px; width: 16px; background: #fff; }
  .sleep-batt-text { font-size: 14px; line-height: 18px; }
  .sleep-divider { position: absolute; z-index: 5; left: var(--sleep-line-x); top: 12px; bottom: 12px; width: 1px; background: #fff; transform: translateX(-1px); }
  @keyframes categoryEnterFromRight { from { opacity: .25; transform: translateX(18px); } to { opacity: 1; transform: translateX(0); } }
  @keyframes categoryEnterFromLeft { from { opacity: .25; transform: translateX(-18px); } to { opacity: 1; transform: translateX(0); } }
  @keyframes itemEnterFromBottom { from { opacity: .35; transform: translateY(8px); } to { opacity: 1; transform: translateY(0); } }
  @keyframes itemEnterFromTop { from { opacity: .35; transform: translateY(-8px); } to { opacity: 1; transform: translateY(0); } }
  @media (prefers-reduced-motion: reduce) { .xmb-axis, .category, .item { transition: none; } .category-rail, .item-list { animation: none !important; } }
</style>
</head>
<body>
<div class="topbar"><button id="tab-xmb" class="active" type="button">XMB · 横纵交叉菜单</button>
  <button id="tab-focus" type="button">打开焦点舞台</button></div>
<div class="stage"><div class="screen" id="screen"><div class="xmb-content" id="xmb-content"><div class="top-status"><span class="battery">▰ 87%</span><span class="clock">08:42</span><span class="date">10月25日 周日</span></div><div class="wave"></div><div class="xmb-axis" id="xmb-axis">
  <div class="category-rail" id="category-rail"></div><div class="rail-rule"></div>
  <div class="section-panel" id="section-panel">
    <div class="item-list" id="item-list"></div>
    <div class="empty-items" id="empty-items" hidden>此分类暂无项目</div></div>
</div></div>
<div class="sleep-layer" id="sleep-layer" aria-hidden="true"><div class="sleep-clock-col">
  <div class="sleep-time-card"><span class="sleep-time-text">08:42</span></div>
  <div class="sleep-date-card"><span class="sleep-date-day">25</span><span class="sleep-date-week">周日</span><span class="sleep-date-month">10月</span><span class="sleep-date-year">2026</span></div>
  <div class="sleep-battery"><div class="sleep-batt-body"><div class="sleep-batt-fill"></div></div><span class="sleep-batt-text">87%</span></div>
</div></div><div class="sleep-divider" id="sleep-divider" aria-hidden="true"></div></div></div>
<div class="keys"><button id="btn-left" type="button">← 分类</button><button id="btn-up" type="button">▲ 上一项</button>
  <button id="btn-ok" type="button">OK 确认</button><button id="btn-down" type="button">▼ 下一项</button><button id="btn-right" type="button">分类 →</button></div>
<div class="preview-tools"><span id="status" role="status" aria-live="polite"></span><span id="power-status" role="status" aria-live="polite"></span><button id="btn-reset" type="button">重置焦点</button><button id="btn-sleep" type="button">预览浅睡</button></div>
<div class="hint" id="hint" role="status" aria-live="polite">← / → 切换分类，↑ / ↓ 选择项目，Enter 确认，Left 返回。</div>
<div class="note">参考 OpenXMB 的布局：顶部状态信息，横向分类图标，红框区域统一使用 14 号文字；子项目从下方展开并移除横线，使用 14 号数字索引和文字。没有子分类的主分类隐藏下方列表，图标使用固件真实点阵资源。4 秒停止图标，20 秒进入反色浅睡时钟。</div>
<script id="firmware-icons" type="application/json">__ICONS__</script>
<script>
const ICONS = JSON.parse(document.getElementById("firmware-icons").textContent);
const CATEGORIES = [
  { name: "设置", items: ["重启设备", "系统更新", "WiFi配网", "时间校准", "设备自检", "USB串口", "恢复默认设置", "关于设备", "关于作者"] },
  { name: "音乐", items: ["音乐列表", "收藏歌曲"] },
  { name: "阅读", items: ["单词阅读", "提词器"] },
  { name: "专注时钟", items: [] },
  { name: "无线功能", items: ["蓝牙连接", "蓝牙拍照", "翻页器"] },
  { name: "游戏", items: ["敲木鱼", "飞机世界大战"] },
  { name: "闹钟", items: [] },
  { name: "课程表", items: [] },
  { name: "Api统计板", items: [] },
  { name: "答案之书", items: [] },
  { name: "今日抽签", items: [] },
  { name: "电脑状态看板", items: [] },
  { name: "SD卡转USB", items: [] },
];
if (ICONS.length !== CATEGORIES.length || ICONS.some((icon, i) => icon.label !== CATEGORIES[i].name)) throw new Error("固件图标和 XMB 分类不一致");
const ICON_IDLE_MS = 4000;
const SLEEP_IDLE_MS = 20000, SLEEP_SLIDE_MS = 720, SLEEP_SHIFT_PX = 256;
let categoryIndex = 0, itemIndex = 0, iconTimer = null, iconFrames = [], iconContext = null, lastFrame = -1, lastActivity = performance.now();
let powerState = "active", idleTimer = null, clockTimer = null, slideFrame = null, sleepProgress = 0, slideTarget = 0, slideStartMs = 0, slideFrom = 0, slideDurationMs = 0, wakeKey = null;
const rail = document.getElementById("category-rail"), list = document.getElementById("item-list"), axis = document.getElementById("xmb-axis");
const panel = document.getElementById("section-panel"), empty = document.getElementById("empty-items");
const status = document.getElementById("status"), hint = document.getElementById("hint"), reducedMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
const screen = document.getElementById("screen"), content = document.getElementById("xmb-content"), sleepLayer = document.getElementById("sleep-layer"), sleepDivider = document.getElementById("sleep-divider"), powerButton = document.getElementById("btn-sleep"), powerStatus = document.getElementById("power-status");
const mod = (n, length) => ((n % length) + length) % length;
const selectedItems = () => CATEGORIES[categoryIndex].items;
function drawFrame(frame) { if (frame === lastFrame) return; iconContext.putImageData(iconFrames[frame], 0, 0); lastFrame = frame; }
function decodeFrame(icon, encoded) {
  const bytes = atob(encoded), size = icon.width, stride = Math.ceil(size / 8), image = iconContext.createImageData(size, size);
  for (let y=0;y<size;y++) for (let x=0;x<size;x++) { const on = bytes.charCodeAt(y*stride+(x>>3)) & (128>>(x&7)); const p=(y*size+x)*4;
    image.data[p]=image.data[p+1]=image.data[p+2]=255; image.data[p+3]=on?255:0; }
  return image;
}
function drawCategoryPosters() {
  rail.querySelectorAll(".category").forEach((node, offset) => {
    const icon = ICONS[mod(categoryIndex + offset, CATEGORIES.length)];
    const canvas = node.querySelector("canvas"), context = canvas.getContext("2d");
    iconContext = context;
    context.putImageData(decodeFrame(icon, icon.frames[icon.posterFrame]), 0, 0);
  });
}
function prepareIcon() {
  if (powerState !== "active") return;
  window.clearTimeout(iconTimer); iconTimer = null; const icon = ICONS[categoryIndex];
  const canvas = rail.querySelector(".category.focus canvas"); if (!canvas) return;
  drawCategoryPosters();
  iconContext = canvas.getContext("2d");
  iconFrames = icon.frames.map(encoded => decodeFrame(icon, encoded));
  lastFrame = -1; drawFrame(reducedMotion.matches ? icon.posterFrame : 0);
  if (reducedMotion.matches || iconFrames.length < 2) return;
  let frame = 1; const tick = () => { iconTimer=null; if (reducedMotion.matches || performance.now()-lastActivity >= ICON_IDLE_MS) return; drawFrame(frame++ % iconFrames.length); iconTimer=window.setTimeout(tick, icon.delay); }; iconTimer=window.setTimeout(tick, icon.delay);
}
function stopIconAnimation() { window.clearTimeout(iconTimer); iconTimer = null; }
function setPowerState(state) {
  powerState = state;
  const labels = { active: "主菜单 · 图标播放中（4 秒无操作后静止）", frozen: "主菜单 · 图标已静止（20 秒无操作后浅睡）", entering: "进入浅睡 · 扫描线右移并切换反色时钟", asleep: "反色浅睡时钟 · 按任意键唤醒", waking: "正在唤醒 · 恢复 XMB 主菜单" };
  powerStatus.textContent = labels[state];
  powerButton.disabled = state === "entering" || state === "asleep" || state === "waking";
}
function clearIdleTimer() { window.clearTimeout(idleTimer); idleTimer = null; }
function reconcileIdle() {
  clearIdleTimer();
  if (document.hidden || !["active", "frozen"].includes(powerState)) return;
  const idle = performance.now() - lastActivity;
  if (idle >= SLEEP_IDLE_MS) { enterSleep(); return; }
  if (idle >= ICON_IDLE_MS) { stopIconAnimation(); setPowerState("frozen"); }
  const deadline = powerState === "active" ? ICON_IDLE_MS : SLEEP_IDLE_MS;
  idleTimer = window.setTimeout(reconcileIdle, Math.max(1, deadline - idle));
}
function updateClockText() {
  const now = new Date(), two = value => String(value).padStart(2, "0"), weekday = "日一二三四五六"[now.getDay()];
  const time = `${two(now.getHours())}:${two(now.getMinutes())}`;
  const date = `${now.getMonth() + 1}月${now.getDate()}日 周${weekday}`;
  document.querySelector(".clock").textContent = time;
  document.querySelector(".date").textContent = date;
  document.querySelector(".sleep-time-text").textContent = time;
  document.querySelector(".sleep-date-day").textContent = two(now.getDate());
  document.querySelector(".sleep-date-week").textContent = `周${weekday}`;
  document.querySelector(".sleep-date-month").textContent = `${now.getMonth() + 1}月`;
  document.querySelector(".sleep-date-year").textContent = now.getFullYear();
  window.clearTimeout(clockTimer);
  if (!document.hidden) clockTimer = window.setTimeout(updateClockText, 60000 - (now.getSeconds() * 1000 + now.getMilliseconds()));
}
function applySleepProgress(progress) {
  sleepProgress = Math.max(0, Math.min(1, progress));
  screen.style.setProperty("--sleep-shift", `${sleepProgress * SLEEP_SHIFT_PX}px`);
  screen.style.setProperty("--sleep-reveal", `${sleepProgress * 100}%`);
  screen.style.setProperty("--sleep-line-x", `${sleepProgress * 384}px`);
  const sleeping = sleepProgress === 1;
  content.inert = sleeping;
  sleepLayer.setAttribute("aria-hidden", sleepProgress === 0 ? "true" : "false");
  sleepDivider.hidden = sleeping;
}
function stopSlide() { window.cancelAnimationFrame(slideFrame); slideFrame = null; }
function finishSlide() {
  stopSlide(); applySleepProgress(slideTarget);
  if (slideTarget === 1) { setPowerState("asleep"); return; }
  lastActivity = performance.now(); setPowerState("active"); render(); updateClockText(); startAfterWake(); reconcileIdle();
}
function startAfterWake() { if (!document.hidden && powerState === "active") prepareIcon(); }
function tickSlide(nowMs) {
  slideFrame = null;
  if (document.hidden) return;
  const t = Math.min(1, (nowMs - slideStartMs) / slideDurationMs);
  const eased = t < .5 ? 4 * t * t * t : 1 - Math.pow(-2 * t + 2, 3) / 2;
  applySleepProgress(slideFrom + (slideTarget - slideFrom) * eased);
  if (t >= 1) finishSlide(); else slideFrame = window.requestAnimationFrame(tickSlide);
}
function slideTo(target) {
  stopSlide(); stopIconAnimation(); clearIdleTimer();
  slideTarget = target; slideFrom = sleepProgress; slideStartMs = performance.now();
  slideDurationMs = SLEEP_SLIDE_MS * Math.abs(target - slideFrom);
  setPowerState(target === 1 ? "entering" : "waking");
  if (reducedMotion.matches || slideDurationMs === 0) finishSlide();
  else if (!document.hidden) slideFrame = window.requestAnimationFrame(tickSlide);
}
function enterSleep() { if (["active", "frozen"].includes(powerState)) slideTo(1); }
function restartAnimation(node, className) {
  node.classList.remove("category-enter-from-right", "category-enter-from-left", "item-enter-from-bottom", "item-enter-from-top");
  if (reducedMotion.matches || !className) return;
  void node.offsetWidth;
  node.classList.add(className);
  node.addEventListener("animationend", () => node.classList.remove(className), { once: true });
}
function renderCategories(direction = "") {
  rail.innerHTML = ""; const focusCenter = 48, step = 70;
  // 焦点图标中心 x=48，与下方子分类图标中心一致；其余分类向右展开。
  for (let offset=0;offset<=4;offset++) { const i=mod(categoryIndex+offset,CATEGORIES.length), c=CATEGORIES[i], node=document.createElement("div");
    node.className="category"+(offset===0?" focus":""); node.style.left=`${focusCenter+step*offset-(offset===0?38:29)}px`;
    node.innerHTML=`<canvas width="42" height="42" aria-hidden="true"></canvas><span class="category-label">${c.name}</span>`; rail.appendChild(node); }
  restartAnimation(rail, direction === "right" ? "category-enter-from-right" : direction === "left" ? "category-enter-from-left" : "");
}
function renderItems(direction = "") {
  const items=selectedItems(); list.innerHTML=""; panel.hidden=items.length===0; empty.hidden=true;
  if (!items.length) return;
  itemIndex=Math.max(0,Math.min(itemIndex,items.length-1)); for(let offset=-2;offset<=1;offset++){const i=itemIndex+offset;if(i<0||i>=items.length)continue;const node=document.createElement("div");
    node.className="item"+(offset===0?" focus":""); node.style.transform=`translateY(${offset*25+19}px)`; node.innerHTML=`<span class="item-index">${String(i+1).padStart(2,"0")}</span><span class="item-name">${items[i]}</span>`; list.appendChild(node);}
  restartAnimation(list, direction === "down" ? "item-enter-from-bottom" : direction === "up" ? "item-enter-from-top" : "");
}
function render(direction = "") { renderCategories(direction); renderItems("category"); prepareIcon(); status.textContent=`${CATEGORIES[categoryIndex].name} · ${selectedItems()[itemIndex]||"空"}`; }
function touch() { lastActivity=performance.now(); if (!iconTimer && !reducedMotion.matches) prepareIcon(); }
function moveCategory(delta) { const next=mod(categoryIndex+delta,CATEGORIES.length); if(next===categoryIndex)return; touch(); categoryIndex=next; itemIndex=0; render(delta>0?"right":"left"); }
function moveItem(delta) { const count=selectedItems().length; if(!count)return; const next=Math.max(0,Math.min(itemIndex+delta,count-1)); if(next===itemIndex)return; touch(); itemIndex=next; renderItems(delta>0?"down":"up"); status.textContent=`${CATEGORIES[categoryIndex].name} · ${selectedItems()[itemIndex]}`; }
function confirm() { hint.textContent=`已确认：${CATEGORIES[categoryIndex].name} → ${selectedItems()[itemIndex]||"空"}（预览演示）`; }
function input(key) { if(key==="left")moveCategory(-1); else if(key==="right")moveCategory(1); else if(key==="up")moveItem(-1); else if(key==="down")moveItem(1); else if(key==="ok") { touch(); confirm(); } else if(key==="back") { touch(); hint.textContent="XMB 根菜单已是顶层。"; } }
function handleDeviceKey(key) {
  if (powerState === "entering" || powerState === "asleep") { slideTo(0); hint.textContent = "已唤醒，当前 XMB 焦点保持不变。"; return true; }
  if (powerState === "waking") return true;
  lastActivity = performance.now(); setPowerState("active"); input(key); reconcileIdle(); return false;
}
document.getElementById("btn-left").onclick=()=>handleDeviceKey("left"); document.getElementById("btn-right").onclick=()=>handleDeviceKey("right"); document.getElementById("btn-up").onclick=()=>handleDeviceKey("up"); document.getElementById("btn-down").onclick=()=>handleDeviceKey("down"); document.getElementById("btn-ok").onclick=()=>handleDeviceKey("ok");
document.getElementById("btn-reset").onclick=()=>{if(powerState!=="active"&&powerState!=="frozen")return; categoryIndex=0;itemIndex=0;lastActivity=performance.now();setPowerState("active");render();hint.textContent="已重置到设置分类。";reconcileIdle();};
powerButton.onclick = enterSleep;
document.getElementById("tab-focus").onclick=()=>{window.location.href="main_menu_mock.html";};
window.addEventListener("keydown", e=>{const key={ArrowLeft:"left",ArrowRight:"right",ArrowUp:"up",ArrowDown:"down",Enter:"ok",Backspace:"back"}[e.key];if(wakeKey===(e.code||e.key)){e.preventDefault();return;}if(!key&&["active","frozen"].includes(powerState))return;e.preventDefault();if(e.repeat)return;if(handleDeviceKey(key||"wake"))wakeKey=e.code||e.key;});
window.addEventListener("keyup", e=>{if((e.code||e.key)===wakeKey)wakeKey=null;});
window.addEventListener("blur",()=>{wakeKey=null;});
window.addEventListener("resize",()=>document.documentElement.style.setProperty("--s",Math.min(2,(document.documentElement.clientWidth-32)/384)));
function suspendPreview() { stopIconAnimation(); clearIdleTimer(); stopSlide(); window.clearTimeout(clockTimer); clockTimer=null; }
function resumePreview() { if(document.hidden)return; updateClockText(); if(powerState==="entering"||powerState==="waking")finishSlide(); else { reconcileIdle(); if(powerState==="active")prepareIcon(); } }
document.addEventListener("visibilitychange",()=>document.hidden?suspendPreview():resumePreview());
document.documentElement.style.setProperty("--s",Math.min(2,(document.documentElement.clientWidth-32)/384)); updateClockText(); render(); setPowerState("active"); reconcileIdle();
</script>
</body>
</html>
'''


def main():
    source = SOURCE.read_text(encoding="utf-8")
    match = re.search(r'<script id="firmware-icons" type="application/json">([\s\S]*?)</script>', source)
    if not match:
        raise SystemExit("Firmware icon export is missing from the focus preview")
    icons = json.loads(match.group(1))
    html = TEMPLATE.replace("__ICONS__", json.dumps(icons, ensure_ascii=False, separators=(",", ":")))
    OUTPUT.write_text(html, encoding="utf-8", newline="\n")
    print(f"Generated {OUTPUT.name}: {len(icons)} categories")


if __name__ == "__main__":
    main()
