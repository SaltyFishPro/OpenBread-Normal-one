#include "WifiPortalPage.h"

#include <Arduino.h>

// Generated from tools/wifi_portal/index.html by tools/wifi_portal/build.py.
// The page is served from flash and streamed with WebServer::send_P.
const char kWifiPortalPage[] PROGMEM = R"WIFIPORTAL(<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#ededeb">
<meta name="format-detection" content="telephone=no">
<title>OpenBread — WiFi 配网</title>
<link rel="icon" href="data:,">
<style>
  :root{
    --paper:#ededeb;
    --paper-light:#f7f7f4;
    --paper-dark:#d7d7d2;
    --ink:#121212;
    --muted:#656560;
    --line:#b9b9b2;
    --line-soft:#d5d5ce;
    --orange:#ff5c22;
    --orange-dark:#d83c0c;
    --green:#135b48;
    --ok:#156a47;
    --warn:#a34317;
    --mono:"SFMono-Regular","Cascadia Mono","Roboto Mono","Liberation Mono",Consolas,monospace;
    --sans:"Helvetica Neue",Helvetica,Arial,"PingFang SC","Hiragino Sans GB","Microsoft YaHei",sans-serif;
  }
  *{box-sizing:border-box}
  html{-webkit-text-size-adjust:100%}
  body{
    margin:0;background:var(--paper);color:var(--ink);font-family:var(--sans);
    min-height:100vh;display:flex;flex-direction:column;
  }
  body::before{
    content:"";position:fixed;inset:0;pointer-events:none;z-index:0;
    background-image:radial-gradient(rgba(18,18,18,.13) .55px,transparent .55px);
    background-size:7px 7px;opacity:.22;
  }
  button,input{font:inherit;color:inherit}
  button{cursor:pointer}
  .mono{font-family:var(--mono)}
  [hidden]{display:none!important}

  /* ---------- top bar ---------- */
  .site-head{
    position:relative;z-index:2;flex:0 0 auto;height:54px;
    border-bottom:1px solid var(--ink);background:var(--paper);
    padding:0 16px;
  }
  .head-inner{
    max-width:560px;height:100%;margin:0 auto;
    display:flex;align-items:center;justify-content:space-between;gap:14px;
  }
  .brand{display:flex;align-items:center;gap:9px;min-width:0}
  .brand-mark{
    width:24px;height:24px;flex:0 0 auto;display:grid;place-items:center;
    background:var(--ink);color:var(--paper-light);
    font:700 8px/1 var(--mono);letter-spacing:.06em;
  }
  .brand-name{font:700 10px/1 var(--mono);letter-spacing:.2em;white-space:nowrap}
  .brand-sub{font:8px/1 var(--mono);letter-spacing:.12em;color:var(--muted);white-space:nowrap}
  .head-status{
    display:flex;align-items:center;gap:7px;flex:0 0 auto;
    font:8.5px/1 var(--mono);letter-spacing:.1em;color:var(--muted);white-space:nowrap;
  }
  .head-status b{font-weight:400;color:var(--ink);font-variant-numeric:tabular-nums}
  .live-dot{width:5px;height:5px;border-radius:50%;background:var(--ok);flex:0 0 auto}
  .live-dot.is-warn{background:var(--orange)}
  .live-dot.is-off{background:var(--line)}

  /* ---------- page ---------- */
  .page{
    position:relative;z-index:1;width:100%;max-width:560px;margin:0 auto;
    padding:28px 16px 30px;flex:1 0 auto;
  }
  .intro{margin-bottom:22px}
  h1{
    margin:0 0 10px;font-size:clamp(28px,7.4vw,34px);line-height:1.1;
    letter-spacing:-.03em;font-weight:700;
  }
  .intro p{
    margin:0;font-size:13.5px;line-height:1.75;color:#3b3b37;
  }

  /* ---------- card ---------- */
  .tool{
    background:var(--paper-light);border:1px solid var(--ink);
    box-shadow:6px 6px 0 rgba(18,18,18,.1);
  }
  .tool-bar{
    display:flex;align-items:center;justify-content:space-between;gap:12px;
    min-height:44px;padding:0 16px;border-bottom:1px solid var(--ink);
    font:9px/1 var(--mono);letter-spacing:.16em;
  }
  .chip{
    display:inline-flex;align-items:center;gap:6px;padding:5px 7px;
    border:1px solid var(--line);background:var(--paper);
    font:8px/1 var(--mono);letter-spacing:.1em;color:var(--muted);white-space:nowrap;
  }
  .chip i{width:5px;height:5px;border-radius:50%;background:var(--orange);flex:0 0 auto}
  .chip b{font-weight:400;color:var(--ink)}
  .chip[data-tone="ok"]{border-color:rgba(21,106,71,.45);color:var(--ok);background:rgba(21,106,71,.06)}
  .chip[data-tone="ok"] i{background:var(--ok)}
  .chip[data-tone="warn"]{border-color:rgba(163,67,23,.45);color:var(--warn);background:rgba(163,67,23,.06)}
  .chip[data-tone="warn"] i{background:var(--warn)}
  .chip[data-tone="muted"] i{background:var(--muted)}
  .chip[data-tone="busy"] i{animation:pulse 1.2s ease-in-out infinite}

  .tool-body{padding:20px 16px 22px;min-height:330px;display:flex;flex-direction:column}

  /* ---------- views ---------- */
  .view{display:none;flex:1;flex-direction:column;animation:view-in .24s ease-out both}
  .view.is-on{display:flex}
  .view-head{margin-bottom:18px}
  .view-head h2{margin:0;font-size:20px;line-height:1.2;letter-spacing:-.02em;font-weight:700}

  .field{display:block;margin-bottom:14px}
  .label-row{
    display:flex;align-items:baseline;justify-content:space-between;gap:10px;
    margin-bottom:7px;font:8.5px/1 var(--mono);letter-spacing:.1em;color:var(--muted);
  }
  .label-row .aside{font-size:7.5px;letter-spacing:.04em;color:var(--line)}
  .input-wrap{position:relative;display:flex}
  .field input{
    width:100%;height:48px;border:1px solid var(--ink);border-radius:0;background:#fff;
    padding:0 13px;font-size:15px;line-height:1;appearance:none;outline:none;
    transition:box-shadow .12s ease;
  }
  .field input::placeholder{color:#9c9c95;font-size:13px}
  .field input:focus{box-shadow:inset 0 0 0 2px var(--orange)}
  .field input:disabled{background:var(--paper);color:var(--muted)}
  .field.has-action input{padding-right:60px}
  .reveal{
    position:absolute;right:0;top:0;height:48px;padding:0 12px;border:0;border-left:1px solid var(--line);
    background:transparent;font:8px/1 var(--mono);letter-spacing:.1em;color:var(--muted);
  }
  .reveal:hover{background:var(--paper);color:var(--ink)}
  .form-error{
    display:flex;gap:7px;align-items:flex-start;margin:-2px 0 12px;
    font-size:11px;line-height:1.6;color:var(--warn);
  }
  .form-error::before{
    content:"!";flex:0 0 auto;width:13px;height:13px;margin-top:1px;border:1px solid currentColor;
    font:700 8px/13px var(--mono);text-align:center;
  }

  .btn{
    position:relative;display:flex;align-items:center;justify-content:space-between;gap:12px;
    width:100%;height:52px;padding:0 16px;border:1px solid var(--ink);border-radius:0;
    background:transparent;font-size:13px;font-weight:700;letter-spacing:.02em;
    transition:transform .12s ease,filter .12s ease;
  }
  .btn svg{width:15px;height:15px;flex:0 0 auto}
  .btn-primary{
    background:var(--orange);border-color:var(--ink);
    clip-path:polygon(0 0,100% 0,100% calc(100% - 12px),calc(100% - 12px) 100%,0 100%);
    filter:drop-shadow(3px 3px 0 var(--ink));
  }
  .btn-primary:hover{background:#ff6b36;filter:drop-shadow(5px 5px 0 var(--ink));transform:translate(-1px,-1px)}
  .btn-primary:active{filter:drop-shadow(1px 1px 0 var(--ink));transform:translate(1px,1px)}
  .btn-secondary{
    background:var(--paper-light);
    clip-path:polygon(0 0,100% 0,100% calc(100% - 10px),calc(100% - 10px) 100%,0 100%);
  }
  .btn-secondary:hover{background:var(--paper)}
  .btn[disabled]{opacity:.55;pointer-events:none}
  .fineprint{
    margin:11px 0 0;font:9px/1.65 var(--mono);letter-spacing:.02em;color:var(--muted);
  }

  /* ---------- connecting ---------- */
  .target{
    display:flex;align-items:center;justify-content:space-between;gap:14px;
    padding:12px 13px;margin-bottom:16px;border:1px solid var(--line);background:var(--paper);min-width:0;
  }
  .target span{font:8px/1 var(--mono);letter-spacing:.13em;color:var(--muted);flex:0 0 auto}
  .target strong{
    font-size:14px;line-height:1.3;text-align:right;font-weight:700;
    white-space:nowrap;overflow:hidden;text-overflow:ellipsis;
  }
  .progress{height:10px;padding:2px;border:1px solid var(--ink);background:#fff;margin-bottom:10px;overflow:hidden}
  .progress span{
    display:block;width:42%;height:100%;
    background:repeating-linear-gradient(135deg,var(--orange) 0 4px,#ff8352 4px 8px);
    animation:stripes .9s linear infinite;
  }
  .status-line{display:flex;align-items:center;gap:7px;margin:0 0 4px;font:10px/1.4 var(--mono);color:var(--muted)}
  .status-line .time{margin-left:auto;color:var(--ink);font-variant-numeric:tabular-nums}
  .pulse{width:6px;height:6px;border-radius:50%;background:var(--orange);flex:0 0 auto;animation:pulse 1.2s ease-in-out infinite}

  /* ---------- result ---------- */
  .result{display:flex;align-items:center;gap:13px;margin:2px 0 18px}
  .result-icon{
    width:44px;height:44px;flex:0 0 auto;display:grid;place-items:center;
    border:1px solid var(--ink);background:var(--paper);
  }
  .result-icon svg{width:21px;height:21px}
  .result-icon.ok{background:var(--green);border-color:var(--green);color:var(--paper-light)}
  .result-icon.warn{border-color:var(--warn);color:var(--warn);background:rgba(163,67,23,.06)}
  .result-icon.muted{color:var(--muted);background:var(--paper)}
  .result-icon .bang{font:700 20px/1 var(--mono)}
  .result-icon .cross{font:400 24px/1 var(--sans)}
  .result-title{margin:0 0 5px;font-size:17px;line-height:1.25;letter-spacing:-.02em;font-weight:700}
  .result-sub{margin:0;font-size:11px;line-height:1.6;color:var(--muted)}
  .result-sub b{color:var(--ink);font-variant-numeric:tabular-nums}

  .kv{display:flex;align-items:baseline;justify-content:space-between;gap:16px;padding:11px 0;border-top:1px solid var(--line-soft)}
  .kv:first-of-type{border-top:1px solid var(--ink)}
  .kv span{font:8px/1 var(--mono);letter-spacing:.13em;color:var(--muted);flex:0 0 auto}
  .kv strong{
    font-size:12px;line-height:1.4;text-align:right;font-weight:700;
    white-space:nowrap;overflow:hidden;text-overflow:ellipsis;
  }
  .kv strong.ip{font:14px/1.4 var(--mono);letter-spacing:.02em}

  .note{
    margin:14px 0 0;padding:10px 12px;border-left:3px solid var(--ink);background:var(--paper);
    font-size:10.5px;line-height:1.7;color:#41413d;
  }
  .note b{font-weight:700;color:var(--ink)}

  /* ---------- device line / footer ---------- */
  .device-line{
    display:flex;flex-wrap:wrap;align-items:center;gap:5px 7px;margin:18px 0 0;
    font:8.5px/1.6 var(--mono);letter-spacing:.04em;color:var(--muted);
  }
  .device-line b{font-weight:400;color:var(--ink)}
  .device-line .sep{color:var(--line)}
  .site-foot{
    position:relative;z-index:1;flex:0 0 auto;width:100%;max-width:560px;margin:0 auto;
    display:flex;align-items:center;justify-content:space-between;gap:16px;
    padding:0 16px 22px;font:7.5px/1.5 var(--mono);letter-spacing:.12em;color:var(--muted);
  }
  .site-foot .mono{color:var(--ink)}

  @keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
  @keyframes stripes{to{background-position:11.3px 0}}
  @keyframes view-in{from{opacity:0;transform:translateY(4px)}to{opacity:1;transform:none}}

  @media (max-width:380px){
    .page{padding-left:12px;padding-right:12px}
    .tool-bar{padding:0 13px;font-size:8px}
    .tool-body{padding:18px 13px 20px}
    .brand-sub{display:none}
  }
  @media (prefers-reduced-motion:reduce){
    *,*::before,*::after{animation-duration:.01ms!important;animation-iteration-count:1!important;transition-duration:.01ms!important}
  }
</style>
</head>
<body>

<header class="site-head">
  <div class="head-inner">
    <div class="brand">
      <span class="brand-mark" aria-hidden="true">OB</span>
      <span class="brand-name">OPENBREAD</span>
      <span class="brand-sub">NORMAL ONE</span>
    </div>
    <div class="head-status">
      <span class="live-dot" id="head-dot" aria-hidden="true"></span>
      <span id="head-label">热点</span>
      <b id="portal-clock">05:00</b>
    </div>
  </div>
</header>

<main class="page">
  <section class="intro" aria-labelledby="page-title">
    <h1 id="page-title">WiFi 配网页面</h1>
    <p>输入 WiFi 名称和密码，设备会自动连接。配网过程中请保持此页面打开。</p>
  </section>

  <section class="tool" aria-label="WiFi 配网">
    <div class="tool-bar">
      <span>网络设置</span>
      <span class="chip" id="chip" data-tone="busy"><i aria-hidden="true"></i><b id="chip-text">等待输入</b></span>
    </div>

    <div class="tool-body" aria-live="polite">

      <div class="view is-on" data-view="portal">
        <div class="view-head"><h2>填写网络</h2></div>

        <form id="connect-form" novalidate>
          <label class="field">
            <span class="label-row">
              <span>WiFi 名称</span>
              <span class="aside">必填</span>
            </span>
            <input id="ssid" name="ssid" type="text" maxlength="32" autocomplete="off"
                   autocapitalize="none" spellcheck="false" placeholder="例如 Home_2.4G"
                   aria-label="WiFi 名称或 SSID" required>
          </label>

          <label class="field has-action">
            <span class="label-row">
              <span>WiFi 密码</span>
              <span class="aside">开放网络可留空</span>
            </span>
            <span class="input-wrap">
              <input id="pass" name="pass" type="password" maxlength="64" autocomplete="current-password"
                     placeholder="输入路由器密码" aria-label="WiFi 密码">
              <button class="reveal" id="reveal" type="button" aria-label="显示密码">显示</button>
            </span>
          </label>

          <p class="form-error" id="form-error" hidden></p>

          <button class="btn btn-primary" id="submit-btn" type="submit">
            <span>开始连接</span>
            <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" aria-hidden="true">
              <path d="M3 13 13 3M6 3h7v7"/>
            </svg>
          </button>
          <p class="fineprint">密码仅用于连接路由器，不会上传。</p>
        </form>
      </div>

      <div class="view" data-view="connecting">
        <div class="view-head"><h2>正在连接</h2></div>

        <div class="target">
          <span>目标网络</span>
          <strong id="target-name">—</strong>
        </div>

        <div class="progress" role="progressbar" aria-label="连接进度"><span></span></div>
        <p class="status-line">
          <span class="pulse" aria-hidden="true"></span>
          <span id="connect-message">正在连接路由器</span>
          <span class="time" id="connect-elapsed">00:00</span>
        </p>

        <p class="note">请保持此页面打开。手机短暂断开时会自动恢复。</p>
      </div>

      <div class="view" data-view="connected">
        <div class="view-head"><h2>配网完成</h2></div>

        <div class="result">
          <span class="result-icon ok" aria-hidden="true">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2">
              <path d="M4 12.5 9.2 18 20 6.5"/>
            </svg>
          </span>
          <div>
            <p class="result-title">设备已接入网络</p>
            <p class="result-sub" id="connected-hint">热点将在 <b>6</b> 秒后关闭</p>
          </div>
        </div>

        <div class="kv"><span>网络</span><strong id="connected-ssid">—</strong></div>
        <div class="kv"><span>设备 IP</span><strong class="ip" id="connected-ip">—</strong></div>

        <p class="note">可以断开热点并返回设备。</p>
      </div>

      <div class="view" data-view="failed">
        <div class="view-head"><h2>连接失败</h2></div>

        <div class="result">
          <span class="result-icon warn" aria-hidden="true"><span class="bang">!</span></span>
          <div>
            <p class="result-title" id="fail-title">密码错误</p>
            <p class="result-sub" id="fail-sub">请检查 WiFi 密码后重试。</p>
          </div>
        </div>

        <div class="kv"><span>网络</span><strong id="failed-ssid">—</strong></div>

        <button class="btn btn-primary" id="retry-btn" type="button" style="margin-top:16px">
          <span>重新填写</span>
          <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" aria-hidden="true">
            <path d="M13.5 8a5.5 5.5 0 1 1-1.6-3.9M13.5 2.5V5H11"/>
          </svg>
        </button>
        <p class="fineprint">热点仍保持开启，可直接重试。</p>
      </div>

      <div class="view" data-view="timeout">
        <div class="view-head"><h2>热点已关闭</h2></div>

        <div class="result">
          <span class="result-icon muted" aria-hidden="true"><span class="cross">×</span></span>
          <div>
            <p class="result-title">配网超时</p>
            <p class="result-sub">设备已关闭热点以节省电量。</p>
          </div>
        </div>

        <p class="note">在设备上重新进入「设置 → WiFi配网」，然后再次连接设备热点。</p>
      </div>

      <div class="view" data-view="offline">
        <div class="view-head"><h2>连接暂时中断</h2></div>

        <div class="result">
          <span class="result-icon muted" aria-hidden="true">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8">
              <path d="M4 9.5a12 12 0 0 1 16 0M7 13a7.5 7.5 0 0 1 10 0M10 16.5a3 3 0 0 1 4 0"/>
              <path d="M3 3l18 18" stroke-width="1.4"/>
            </svg>
          </span>
          <div>
            <p class="result-title">正在重新连接设备</p>
            <p class="result-sub">请确认手机仍连接设备热点。</p>
          </div>
        </div>

        <button class="btn btn-secondary" id="offline-retry" type="button">
          <span>立即重试</span>
          <svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" aria-hidden="true">
            <path d="M13.5 8a5.5 5.5 0 1 1-1.6-3.9M13.5 2.5V5H11"/>
          </svg>
        </button>
      </div>

    </div>
  </section>

  <p class="device-line">
    <span>设备热点</span><b id="ap-name">OpenBread-Setup-XXXX</b>
    <span class="sep">·</span><span>仅支持 2.4 GHz</span>
  </p>
</main>

<footer class="site-foot">
  <span>OPENBREAD NORMAL ONE</span>
  <span class="mono">192.168.4.1</span>
</footer>

<script>
(function () {
  'use strict';

  var $ = function (sel) { return document.querySelector(sel); };
  var $$ = function (sel) { return Array.prototype.slice.call(document.querySelectorAll(sel)); };
  var views = $$('.view');
  var clockEl = $('#portal-clock');
  var headLabelEl = $('#head-label');
  var headDotEl = $('#head-dot');
  var chipEl = $('#chip');
  var chipTextEl = $('#chip-text');
  var hintEl = $('#connected-hint');
  var offlineRetryEl = $('#offline-retry');

  var view = 'portal';
  var state = { ssid: '', ip: '' };
  var portalAlive = true;
  var portalLeft = 300;
  var holdLeft = 0;
  var pollTimer = 0;
  var pollInFlight = false;
  var submitting = false;
  var submittedAt = 0;
  var offlineMisses = 0;
  var prefilled = false;

  var CHIP = {
    portal:     ['busy', '等待输入'],
    connecting: ['busy', '连接中'],
    connected:  ['ok', '已连接'],
    failed:     ['warn', '连接失败'],
    timeout:    ['muted', '已关闭'],
    offline:    ['muted', '等待设备']
  };
  var ERRORS = {
    auth_failed:  ['密码错误', '请检查 WiFi 密码后重试。'],
    ap_not_found: ['未找到网络', '请确认路由器已开启，且设备在 2.4 GHz 覆盖范围内。'],
    timeout:      ['连接超时', '路由器没有及时响应，请重试。'],
    invalid_input:['信息无效', '网络名称或密码格式不正确。'],
    unknown:      ['未知错误', '请重新尝试；若持续失败，请重启设备。']
  };

  function pad(n) { return n < 10 ? '0' + n : String(n); }

  function formatClock(total) {
    if (total < 0) total = 0;
    return pad(Math.floor(total / 60)) + ':' + pad(total % 60);
  }

  function isClosed() {
    return view === 'timeout' || (view === 'connected' && (!portalAlive || holdLeft <= 0));
  }

  function setChrome() {
    var meta = CHIP[view] || CHIP.portal;
    chipEl.setAttribute('data-tone', meta[0]);
    chipTextEl.textContent = meta[1];
    headLabelEl.textContent = isClosed() ? '热点已关闭' : '热点';
    if (view === 'connected') {
      clockEl.textContent = isClosed() ? '已关闭' : formatClock(holdLeft);
      if (portalAlive && holdLeft > 0) {
        hintEl.innerHTML = '热点将在 <b>' + holdLeft + '</b> 秒后关闭';
      } else {
        hintEl.textContent = '热点已关闭，可返回设备';
      }
    } else {
      clockEl.textContent = view === 'timeout' ? '已关闭' : formatClock(portalLeft);
    }
    headDotEl.classList.toggle('is-warn',
      view === 'connecting' || view === 'failed' ||
      (view === 'connected' && offlineMisses > 0 && portalAlive));
    headDotEl.classList.toggle('is-off', isClosed() || view === 'offline');
  }

  function show(next) {
    view = next;
    views.forEach(function (el) {
      el.classList.toggle('is-on', el.getAttribute('data-view') === next);
    });
    setChrome();
  }

  function setFormEnabled(enabled) {
    $('#ssid').disabled = !enabled;
    $('#pass').disabled = !enabled;
    $('#submit-btn').disabled = !enabled;
  }

  function showPortalForm() {
    setFormEnabled(true);
    $('#ssid').value = state.ssid;
    $('#pass').value = '';
    $('#pass').type = 'password';
    $('#reveal').textContent = '显示';
    $('#reveal').setAttribute('aria-label', '显示密码');
    $('#form-error').hidden = true;
    show('portal');
  }

  function resetForm(keepSsid) {
    submitting = false;
    state.ssid = keepSsid ? state.ssid : '';
    showPortalForm();
    if (!keepSsid) $('#ssid').value = '';
  }

  function fillConnected() {
    $('#connected-ssid').textContent = state.ssid || '—';
    $('#connected-ip').textContent = state.ip || '—';
    if (!portalAlive || holdLeft <= 0) {
      hintEl.textContent = '热点已关闭，可返回设备';
    }
  }

  function showFailed(errorKey) {
    var e = ERRORS[errorKey] || ERRORS.unknown;
    $('#fail-title').textContent = e[0];
    $('#fail-sub').textContent = e[1];
    $('#failed-ssid').textContent = state.ssid || '—';
    show('failed');
  }

  function stopPolling() {
    if (pollTimer) {
      clearTimeout(pollTimer);
      pollTimer = 0;
    }
  }

  function schedulePoll(delayMs) {
    stopPolling();
    if (view === 'timeout' || (view === 'connected' && !portalAlive)) return;
    pollTimer = setTimeout(function () { pollStatus(false); }, delayMs);
  }

  function handleOffline() {
    offlineMisses += 1;
    if (view === 'connected') {
      if (!portalAlive || holdLeft <= 0) {
        hintEl.textContent = '热点已关闭，可返回设备';
        stopPolling();
      }
      setChrome();
      return;
    }
    if (view === 'timeout') return;
    show('offline');
  }

  function applyStatus(data) {
    if (!data || typeof data !== 'object') return;

    if (typeof data.portal === 'boolean') portalAlive = data.portal;
    if (typeof data.ap === 'string' && data.ap) $('#ap-name').textContent = data.ap;
    if (typeof data.target === 'string') state.ssid = data.target;
    if (typeof data.ip === 'string') state.ip = data.ip;
    if (typeof data.remaining === 'number') {
      if (portalLeft <= 0 || data.remaining < portalLeft) portalLeft = data.remaining;
    }
    if (typeof data.hold === 'number') {
      if (view !== 'connected' || holdLeft <= 0 || data.hold < holdLeft) holdLeft = data.hold;
    }

    var next = String(data.state || '');
    if (next === 'portal' && !prefilled) {
      if (!$('#ssid').value && state.ssid) $('#ssid').value = state.ssid;
      prefilled = true;
    }
    if (next === 'portal') {
      if (submitting) {
        if (Date.now() - submittedAt > 4000) {
          submitting = false;
          showPortalForm();
        }
        return;
      }
      if (view !== 'portal') {
        showPortalForm();
      } else {
        setChrome();
      }
      return;
    }

    submitting = false;

    if (next === 'connecting') {
      $('#target-name').textContent = state.ssid || '—';
      show('connecting');
      return;
    }

    if (next === 'connected' || next === 'syncing') {
      fillConnected();
      show('connected');
      if (!portalAlive || next === 'syncing') {
        hintEl.textContent = '热点已关闭，可返回设备';
        stopPolling();
      }
      return;
    }

    if (next === 'failed') {
      showFailed(data.error);
      return;
    }

    if (next === 'timeout') {
      show('timeout');
      stopPolling();
      return;
    }

    if (next === 'idle') {
      show('offline');
      stopPolling();
    }
  }

  function pollStatus(manual) {
    if (pollInFlight) return;
    pollInFlight = true;
    if (manual) offlineRetryEl.disabled = true;

    fetch('/status', { cache: 'no-store' })
      .then(function (response) {
        if (!response.ok) throw new Error('status');
        return response.json();
      })
      .then(function (data) {
        offlineMisses = 0;
        applyStatus(data);
      })
      .catch(function () {
        handleOffline();
      })
      .then(function () {
        pollInFlight = false;
        if (manual) offlineRetryEl.disabled = false;
        if (view !== 'timeout' && !(view === 'connected' && !portalAlive)) {
          schedulePoll(900);
        }
      });
  }

  function pollNow() {
    stopPolling();
    pollStatus(false);
  }

  function focusTool() {
    if (window.innerWidth > 700) return;
    var tool = document.querySelector('.tool');
    var top = tool.getBoundingClientRect().top + window.scrollY - 8;
    if (window.scrollY < top) window.scrollTo({ top: top, behavior: 'smooth' });
  }

  $('#connect-form').addEventListener('submit', function (event) {
    event.preventDefault();
    var ssid = $('#ssid').value;
    var pass = $('#pass').value;
    var error = '';
    var encoder = window.TextEncoder ? new TextEncoder() : null;
    var ssidBytes = encoder ? encoder.encode(ssid).length : ssid.length;
    var passBytes = encoder ? encoder.encode(pass).length : pass.length;

    if (ssid.length === 0) error = '请填写 WiFi 名称。';
    else if (ssidBytes > 32) error = 'WiFi 名称超过 32 字节，请缩短后重试。';
    else if (passBytes > 64) error = 'WiFi 密码超过 64 字节，请检查后重试。';

    if (error) {
      $('#form-error').textContent = error;
      $('#form-error').hidden = false;
      return;
    }

    $('#form-error').hidden = true;
    state.ssid = ssid;
    $('#target-name').textContent = ssid;
    setFormEnabled(false);
    submitting = true;
    submittedAt = Date.now();
    show('connecting');
    focusTool();

    var body = new URLSearchParams();
    body.set('ssid', ssid);
    body.set('pass', pass);
    fetch('/connect', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
      body: body.toString()
    })
      .then(function (response) {
        return response.json().catch(function () { return {}; }).then(function (data) {
          return { ok: response.ok, status: response.status, data: data };
        });
      })
      .then(function (result) {
        if (result.ok) {
          pollNow();
          return;
        }
        if (result.status === 400) {
          submitting = false;
          setFormEnabled(true);
          show('portal');
          $('#form-error').textContent = '网络名称或密码格式不正确。';
          $('#form-error').hidden = false;
          return;
        }
        pollNow();
      })
      .catch(function () {
        handleOffline();
        pollNow();
      });
  });

  $('#reveal').addEventListener('click', function () {
    var pass = $('#pass');
    var showText = pass.type === 'password';
    pass.type = showText ? 'text' : 'password';
    this.textContent = showText ? '隐藏' : '显示';
    this.setAttribute('aria-label', showText ? '隐藏密码' : '显示密码');
  });

  $('#retry-btn').addEventListener('click', function () {
    resetForm(true);
    focusTool();
    setTimeout(function () { $('#pass').focus(); }, 60);
  });

  offlineRetryEl.addEventListener('click', pollNow);

  document.addEventListener('visibilitychange', function () {
    if (!document.hidden && view !== 'timeout') pollNow();
  });

  setInterval(function () {
    if (view === 'connected') {
      if (portalAlive && holdLeft > 0) holdLeft -= 1;
      if (holdLeft <= 0) stopPolling();
    } else if (view !== 'timeout' && portalLeft > 0) {
      portalLeft -= 1;
    }
    setChrome();
  }, 1000);

  show('portal');
  pollStatus(false);
})();
</script>
</body>
</html>
)WIFIPORTAL";
