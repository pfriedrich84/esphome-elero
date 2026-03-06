#pragma once

#ifdef __AVR__
#include <pgmspace.h>
#elif !defined(PROGMEM)
#define PROGMEM
#endif

// AUTO-GENERATED FILE — do not edit by hand.
// To rebuild: cd components/elero_web/frontend && npm run build

namespace esphome {
namespace elero {

const char ELERO_WEB_UI_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Elero Blind Manager</title>
  <script type="module" crossorigin>(function(){const e=document.createElement("link").relList;if(e&&e.supports&&e.supports("modulepreload"))return;for(const i of document.querySelectorAll('link[rel="modulepreload"]'))r(i);new MutationObserver(i=>{for(const s of i)if(s.type==="childList")for(const o of s.addedNodes)o.tagName==="LINK"&&o.rel==="modulepreload"&&r(o)}).observe(document,{childList:!0,subtree:!0});function n(i){const s={};return i.integrity&&(s.integrity=i.integrity),i.referrerPolicy&&(s.referrerPolicy=i.referrerPolicy),i.crossOrigin==="use-credentials"?s.credentials="include":i.crossOrigin==="anonymous"?s.credentials="omit":s.credentials="same-origin",s}function r(i){if(i.ep)return;i.ep=!0;const s=n(i);fetch(i.href,s)}})();var It=!1,Rt=!1,j=[],qt=-1,Jt=!1;function Yn(t){Qn(t)}function Xn(){Jt=!0}function Zn(){Jt=!1,Le()}function Qn(t){j.includes(t)||j.push(t),Le()}function tr(t){let e=j.indexOf(t);e!==-1&&e>qt&&j.splice(e,1)}function Le(){if(!Rt&&!It){if(Jt)return;It=!0,queueMicrotask(er)}}function er(){It=!1,Rt=!0;for(let t=0;t<j.length;t++)j[t](),qt=t;j.length=0,qt=-1,Rt=!1}var Y,z,X,Ie,kt=!0;function nr(t){kt=!1,t(),kt=!0}function rr(t){Y=t.reactive,X=t.release,z=e=>t.effect(e,{scheduler:n=>{kt?Yn(n):n()}}),Ie=t.raw}function ye(t){z=t}function ir(t){let e=()=>{};return[r=>{let i=z(r);return t._x_effects||(t._x_effects=new Set,t._x_runEffects=()=>{t._x_effects.forEach(s=>s())}),t._x_effects.add(i),e=()=>{i!==void 0&&(t._x_effects.delete(i),X(i))},i},()=>{e()}]}function Re(t,e){let n=!0,r,i=z(()=>{let s=t();if(JSON.stringify(s),!n&&(typeof s=="object"||s!==r)){let o=r;queueMicrotask(()=>{e(s,o)})}r=s,n=!1});return()=>X(i)}async function sr(t){Xn();try{await t(),await Promise.resolve()}finally{Zn()}}var qe=[],ke=[],je=[];function or(t){je.push(t)}function Vt(t,e){typeof e=="function"?(t._x_cleanups||(t._x_cleanups=[]),t._x_cleanups.push(e)):(e=t,ke.push(e))}function Ne(t){qe.push(t)}function Fe(t,e,n){t._x_attributeCleanups||(t._x_attributeCleanups={}),t._x_attributeCleanups[e]||(t._x_attributeCleanups[e]=[]),t._x_attributeCleanups[e].push(n)}function De(t,e){t._x_attributeCleanups&&Object.entries(t._x_attributeCleanups).forEach(([n,r])=>{(e===void 0||e.includes(n))&&(r.forEach(i=>i()),delete t._x_attributeCleanups[n])})}function ar(t){var e,n;for((e=t._x_effects)==null||e.forEach(tr);(n=t._x_cleanups)!=null&&n.length;)t._x_cleanups.pop()()}var Yt=new MutationObserver(te),Xt=!1;function Zt(){Yt.observe(document,{subtree:!0,childList:!0,attributes:!0,attributeOldValue:!0}),Xt=!0}function Be(){cr(),Yt.disconnect(),Xt=!1}var nt=[];function cr(){let t=Yt.takeRecords();nt.push(()=>t.length>0&&te(t));let e=nt.length;queueMicrotask(()=>{if(nt.length===e)for(;nt.length>0;)nt.shift()()})}function g(t){if(!Xt)return t();Be();let e=t();return Zt(),e}var Qt=!1,vt=[];function lr(){Qt=!0}function ur(){Qt=!1,te(vt),vt=[]}function te(t){if(Qt){vt=vt.concat(t);return}let e=[],n=new Set,r=new Map,i=new Map;for(let s=0;s<t.length;s++)if(!t[s].target._x_ignoreMutationObserver&&(t[s].type==="childList"&&(t[s].removedNodes.forEach(o=>{o.nodeType===1&&o._x_marker&&n.add(o)}),t[s].addedNodes.forEach(o=>{if(o.nodeType===1){if(n.has(o)){n.delete(o);return}o._x_marker||e.push(o)}})),t[s].type==="attributes")){let o=t[s].target,a=t[s].attributeName,c=t[s].oldValue,l=()=>{r.has(o)||r.set(o,[]),r.get(o).push({name:a,value:o.getAttribute(a)})},u=()=>{i.has(o)||i.set(o,[]),i.get(o).push(a)};o.hasAttribute(a)&&c===null?l():o.hasAttribute(a)?(u(),l()):u()}i.forEach((s,o)=>{De(o,s)}),r.forEach((s,o)=>{qe.forEach(a=>a(o,s))});for(let s of n)e.some(o=>o.contains(s))||ke.forEach(o=>o(s));for(let s of e)s.isConnected&&je.forEach(o=>o(s));e=null,n=null,r=null,i=null}function Ke(t){return K(B(t))}function ft(t,e,n){return t._x_dataStack=[e,...B(n||t)],()=>{t._x_dataStack=t._x_dataStack.filter(r=>r!==e)}}function B(t){return t._x_dataStack?t._x_dataStack:typeof ShadowRoot=="function"&&t instanceof ShadowRoot?B(t.host):t.parentNode?B(t.parentNode):[]}function K(t){return new Proxy({objects:t},fr)}var fr={ownKeys({objects:t}){return Array.from(new Set(t.flatMap(e=>Object.keys(e))))},has({objects:t},e){return e==Symbol.unscopables?!1:t.some(n=>Object.prototype.hasOwnProperty.call(n,e)||Reflect.has(n,e))},get({objects:t},e,n){return e=="toJSON"?dr:Reflect.get(t.find(r=>Reflect.has(r,e))||{},e,n)},set({objects:t},e,n,r){const i=t.find(o=>Object.prototype.hasOwnProperty.call(o,e))||t[t.length-1],s=Object.getOwnPropertyDescriptor(i,e);return s!=null&&s.set&&(s!=null&&s.get)?s.set.call(r,n)||!0:Reflect.set(i,e,n)}};function dr(){return Reflect.ownKeys(this).reduce((e,n)=>(e[n]=Reflect.get(this,n),e),{})}function ee(t){let e=r=>typeof r=="object"&&!Array.isArray(r)&&r!==null,n=(r,i="")=>{Object.entries(Object.getOwnPropertyDescriptors(r)).forEach(([s,{value:o,enumerable:a}])=>{if(a===!1||o===void 0||typeof o=="object"&&o!==null&&o.__v_skip)return;let c=i===""?s:`${i}.${s}`;typeof o=="object"&&o!==null&&o._x_interceptor?r[s]=o.initialize(t,c,s):e(o)&&o!==r&&!(o instanceof Element)&&n(o,c)})};return n(t)}function Ue(t,e=()=>{}){let n={initialValue:void 0,_x_interceptor:!0,initialize(r,i,s){return t(this.initialValue,()=>pr(r,i),o=>jt(r,i,o),i,s)}};return e(n),r=>{if(typeof r=="object"&&r!==null&&r._x_interceptor){let i=n.initialize.bind(n);n.initialize=(s,o,a)=>{let c=r.initialize(s,o,a);return n.initialValue=c,i(s,o,a)}}else n.initialValue=r;return n}}function pr(t,e){return e.split(".").reduce((n,r)=>n[r],t)}function jt(t,e,n){if(typeof e=="string"&&(e=e.split(".")),e.length===1)t[e[0]]=n;else{if(e.length===0)throw error;return t[e[0]]||(t[e[0]]={}),jt(t[e[0]],e.slice(1),n)}}var He={};function $(t,e){He[t]=e}function ct(t,e){let n=hr(e);return Object.entries(He).forEach(([r,i])=>{Object.defineProperty(t,`$${r}`,{get(){return i(e,n)},enumerable:!1})}),t}function hr(t){let[e,n]=Ze(t),r={interceptor:Ue,...e};return Vt(t,n),r}function _r(t,e,n,...r){try{return n(...r)}catch(i){lt(i,t,e)}}function lt(...t){return ze(...t)}var ze=mr;function gr(t){ze=t}function mr(t,e,n=void 0){t=Object.assign(t??{message:"No error message given."},{el:e,expression:n}),console.warn(`Alpine Expression Error: ${t.message}

${n?'Expression: "'+n+`"

`:""}`,e),setTimeout(()=>{throw t},0)}var J=!0;function We(t){let e=J;J=!1;let n=t();return J=e,n}function N(t,e,n={}){let r;return b(t,e)(i=>r=i,n),r}function b(...t){return Ge(...t)}var Ge=Ve;function yr(t){Ge=t}var Je;function vr(t){Je=t}function Ve(t,e){let n={};ct(n,t);let r=[n,...B(t)],i=typeof e=="function"?xr(r,e):br(r,e,t);return _r.bind(null,t,e,i)}function xr(t,e){return(n=()=>{},{scope:r={},params:i=[],context:s}={})=>{if(!J){ut(n,e,K([r,...t]),i);return}let o=e.apply(K([r,...t]),i);ut(n,o)}}var Ct={};function wr(t,e){if(Ct[t])return Ct[t];let n=Object.getPrototypeOf(async function(){}).constructor,r=/^[\n\s]*if.*\(.*\)/.test(t.trim())||/^(let|const)\s/.test(t.trim())?`(async()=>{ ${t} })()`:t,s=(()=>{try{let o=new n(["__self","scope"],`with (scope) { __self.result = ${r} }; __self.finished = true; return __self.result;`);return Object.defineProperty(o,"name",{value:`[Alpine] ${t}`}),o}catch(o){return lt(o,e,t),Promise.resolve()}})();return Ct[t]=s,s}function br(t,e,n){let r=wr(e,n);return(i=()=>{},{scope:s={},params:o=[],context:a}={})=>{r.result=void 0,r.finished=!1;let c=K([s,...t]);if(typeof r=="function"){let l=r.call(a,r,c).catch(u=>lt(u,n,e));r.finished?(ut(i,r.result,c,o,n),r.result=void 0):l.then(u=>{ut(i,u,c,o,n)}).catch(u=>lt(u,n,e)).finally(()=>r.result=void 0)}}}function ut(t,e,n,r,i){if(J&&typeof e=="function"){let s=e.apply(n,r);s instanceof Promise?s.then(o=>ut(t,o,n,r)).catch(o=>lt(o,i,e)):t(s)}else typeof e=="object"&&e instanceof Promise?e.then(s=>t(s)):t(e)}function Sr(...t){return Je(...t)}function Er(t,e,n={}){let r={};ct(r,t);let i=[r,...B(t)],s=K([n.scope??{},...i]),o=n.params??[];if(e.includes("await")){let a=Object.getPrototypeOf(async function(){}).constructor,c=/^[\n\s]*if.*\(.*\)/.test(e.trim())||/^(let|const)\s/.test(e.trim())?`(async()=>{ ${e} })()`:e;return new a(["scope"],`with (scope) { let __result = ${c}; return __result }`).call(n.context,s)}else{let a=/^[\n\s]*if.*\(.*\)/.test(e.trim())||/^(let|const)\s/.test(e.trim())?`(()=>{ ${e} })()`:e,l=new Function(["scope"],`with (scope) { let __result = ${a}; return __result }`).call(n.context,s);return typeof l=="function"&&J?l.apply(s,o):l}}var ne="x-";function Z(t=""){return ne+t}function Tr(t){ne=t}var xt={};function v(t,e){return xt[t]=e,{before(n){if(!xt[n]){console.warn(String.raw`Cannot find directive \`${n}\`. \`${t}\` will use the default order of execution`);return}const r=k.indexOf(n);k.splice(r>=0?r:k.indexOf("DEFAULT"),0,t)}}}function Ar(t){return Object.keys(xt).includes(t)}function re(t,e,n){if(e=Array.from(e),t._x_virtualDirectives){let s=Object.entries(t._x_virtualDirectives).map(([a,c])=>({name:a,value:c})),o=Ye(s);s=s.map(a=>o.find(c=>c.name===a.name)?{name:`x-bind:${a.name}`,value:`"${a.value}"`}:a),e=e.concat(s)}let r={};return e.map(en((s,o)=>r[s]=o)).filter(rn).map(Cr(r,n)).sort(Mr).map(s=>$r(t,s))}function Ye(t){return Array.from(t).map(en()).filter(e=>!rn(e))}var Nt=!1,st=new Map,Xe=Symbol();function Or(t){Nt=!0;let e=Symbol();Xe=e,st.set(e,[]);let n=()=>{for(;st.get(e).length;)st.get(e).shift()();st.delete(e)},r=()=>{Nt=!1,n()};t(n),r()}function Ze(t){let e=[],n=a=>e.push(a),[r,i]=ir(t);return e.push(i),[{Alpine:tt,effect:r,cleanup:n,evaluateLater:b.bind(b,t),evaluate:N.bind(N,t)},()=>e.forEach(a=>a())]}function $r(t,e){let n=()=>{},r=xt[e.type]||n,[i,s]=Ze(t);Fe(t,e.original,s);let o=()=>{t._x_ignore||t._x_ignoreSelf||(r.inline&&r.inline(t,e,i),r=r.bind(r,t,e,i),Nt?st.get(Xe).push(r):r())};return o.runCleanups=s,o}var Qe=(t,e)=>({name:n,value:r})=>(n.startsWith(t)&&(n=n.replace(t,e)),{name:n,value:r}),tn=t=>t;function en(t=()=>{}){return({name:e,value:n})=>{let{name:r,value:i}=nn.reduce((s,o)=>o(s),{name:e,value:n});return r!==e&&t(r,e),{name:r,value:i}}}var nn=[];function ie(t){nn.push(t)}function rn({name:t}){return sn().test(t)}var sn=()=>new RegExp(`^${ne}([^:^.]+)\\b`);function Cr(t,e){return({name:n,value:r})=>{n===r&&(r="");let i=n.match(sn()),s=n.match(/:([a-zA-Z0-9\-_:]+)/),o=n.match(/\.[^.\]]+(?=[^\]]*$)/g)||[],a=e||t[n]||n;return{type:i?i[1]:null,value:s?s[1]:null,modifiers:o.map(c=>c.replace(".","")),expression:r,original:a}}}var Ft="DEFAULT",k=["ignore","ref","data","id","anchor","bind","init","for","model","modelable","transition","show","if",Ft,"teleport"];function Mr(t,e){let n=k.indexOf(t.type)===-1?Ft:t.type,r=k.indexOf(e.type)===-1?Ft:e.type;return k.indexOf(n)-k.indexOf(r)}function ot(t,e,n={}){t.dispatchEvent(new CustomEvent(e,{detail:n,bubbles:!0,composed:!0,cancelable:!0}))}function U(t,e){if(typeof ShadowRoot=="function"&&t instanceof ShadowRoot){Array.from(t.children).forEach(i=>U(i,e));return}let n=!1;if(e(t,()=>n=!0),n)return;let r=t.firstElementChild;for(;r;)U(r,e),r=r.nextElementSibling}function T(t,...e){console.warn(`Alpine Warning: ${t}`,...e)}var ve=!1;function Pr(){ve&&T("Alpine has already been initialized on this page. Calling Alpine.start() more than once can cause problems."),ve=!0,document.body||T("Unable to initialize. Trying to load Alpine before `<body>` is available. Did you forget to add `defer` in Alpine's `<script>` tag?"),ot(document,"alpine:init"),ot(document,"alpine:initializing"),Zt(),or(e=>M(e,U)),Vt(e=>Q(e)),Ne((e,n)=>{re(e,n).forEach(r=>r())});let t=e=>!bt(e.parentElement,!0);Array.from(document.querySelectorAll(cn().join(","))).filter(t).forEach(e=>{M(e)}),ot(document,"alpine:initialized"),setTimeout(()=>{qr()})}var se=[],on=[];function an(){return se.map(t=>t())}function cn(){return se.concat(on).map(t=>t())}function ln(t){se.push(t)}function un(t){on.push(t)}function bt(t,e=!1){return H(t,n=>{if((e?cn():an()).some(i=>n.matches(i)))return!0})}function H(t,e){if(t){if(e(t))return t;if(t._x_teleportBack&&(t=t._x_teleportBack),t.parentNode instanceof ShadowRoot)return H(t.parentNode.host,e);if(t.parentElement)return H(t.parentElement,e)}}function Lr(t){return an().some(e=>t.matches(e))}var fn=[];function Ir(t){fn.push(t)}var Rr=1;function M(t,e=U,n=()=>{}){H(t,r=>r._x_ignore)||Or(()=>{e(t,(r,i)=>{r._x_marker||(n(r,i),fn.forEach(s=>s(r,i)),re(r,r.attributes).forEach(s=>s()),r._x_ignore||(r._x_marker=Rr++),r._x_ignore&&i())})})}function Q(t,e=U){e(t,n=>{ar(n),De(n),delete n._x_marker})}function qr(){[["ui","dialog",["[x-dialog], [x-popover]"]],["anchor","anchor",["[x-anchor]"]],["sort","sort",["[x-sort]"]]].forEach(([e,n,r])=>{Ar(n)||r.some(i=>{if(document.querySelector(i))return T(`found "${i}", but missing ${e} plugin`),!0})})}var Dt=[],oe=!1;function ae(t=()=>{}){return queueMicrotask(()=>{oe||setTimeout(()=>{Bt()})}),new Promise(e=>{Dt.push(()=>{t(),e()})})}function Bt(){for(oe=!1;Dt.length;)Dt.shift()()}function kr(){oe=!0}function ce(t,e){return Array.isArray(e)?xe(t,e.join(" ")):typeof e=="object"&&e!==null?jr(t,e):typeof e=="function"?ce(t,e()):xe(t,e)}function xe(t,e){let n=i=>i.split(" ").filter(s=>!t.classList.contains(s)).filter(Boolean),r=i=>(t.classList.add(...i),()=>{t.classList.remove(...i)});return e=e===!0?e="":e||"",r(n(e))}function jr(t,e){let n=a=>a.split(" ").filter(Boolean),r=Object.entries(e).flatMap(([a,c])=>c?n(a):!1).filter(Boolean),i=Object.entries(e).flatMap(([a,c])=>c?!1:n(a)).filter(Boolean),s=[],o=[];return i.forEach(a=>{t.classList.contains(a)&&(t.classList.remove(a),o.push(a))}),r.forEach(a=>{t.classList.contains(a)||(t.classList.add(a),s.push(a))}),()=>{o.forEach(a=>t.classList.add(a)),s.forEach(a=>t.classList.remove(a))}}function St(t,e){return typeof e=="object"&&e!==null?Nr(t,e):Fr(t,e)}function Nr(t,e){let n={};return Object.entries(e).forEach(([r,i])=>{n[r]=t.style[r],r.startsWith("--")||(r=Dr(r)),t.style.setProperty(r,i)}),setTimeout(()=>{t.style.length===0&&t.removeAttribute("style")}),()=>{St(t,n)}}function Fr(t,e){let n=t.getAttribute("style",e);return t.setAttribute("style",e),()=>{t.setAttribute("style",n||"")}}function Dr(t){return t.replace(/([a-z])([A-Z])/g,"$1-$2").toLowerCase()}function Kt(t,e=()=>{}){let n=!1;return function(){n?e.apply(this,arguments):(n=!0,t.apply(this,arguments))}}v("transition",(t,{value:e,modifiers:n,expression:r},{evaluate:i})=>{typeof r=="function"&&(r=i(r)),r!==!1&&(!r||typeof r=="boolean"?Kr(t,n,e):Br(t,r,e))});function Br(t,e,n){dn(t,ce,""),{enter:i=>{t._x_transition.enter.during=i},"enter-start":i=>{t._x_transition.enter.start=i},"enter-end":i=>{t._x_transition.enter.end=i},leave:i=>{t._x_transition.leave.during=i},"leave-start":i=>{t._x_transition.leave.start=i},"leave-end":i=>{t._x_transition.leave.end=i}}[n](e)}function Kr(t,e,n){dn(t,St);let r=!e.includes("in")&&!e.includes("out")&&!n,i=r||e.includes("in")||["enter"].includes(n),s=r||e.includes("out")||["leave"].includes(n);e.includes("in")&&!r&&(e=e.filter((d,m)=>m<e.indexOf("out"))),e.includes("out")&&!r&&(e=e.filter((d,m)=>m>e.indexOf("out")));let o=!e.includes("opacity")&&!e.includes("scale"),a=o||e.includes("opacity"),c=o||e.includes("scale"),l=a?0:1,u=c?rt(e,"scale",95)/100:1,p=rt(e,"delay",0)/1e3,w=rt(e,"origin","center"),S="opacity, transform",A=rt(e,"duration",150)/1e3,h=rt(e,"duration",75)/1e3,f="cubic-bezier(0.4, 0.0, 0.2, 1)";i&&(t._x_transition.enter.during={transformOrigin:w,transitionDelay:`${p}s`,transitionProperty:S,transitionDuration:`${A}s`,transitionTimingFunction:f},t._x_transition.enter.start={opacity:l,transform:`scale(${u})`},t._x_transition.enter.end={opacity:1,transform:"scale(1)"}),s&&(t._x_transition.leave.during={transformOrigin:w,transitionDelay:`${p}s`,transitionProperty:S,transitionDuration:`${h}s`,transitionTimingFunction:f},t._x_transition.leave.start={opacity:1,transform:"scale(1)"},t._x_transition.leave.end={opacity:l,transform:`scale(${u})`})}function dn(t,e,n={}){t._x_transition||(t._x_transition={enter:{during:n,start:n,end:n},leave:{during:n,start:n,end:n},in(r=()=>{},i=()=>{}){Ut(t,e,{during:this.enter.during,start:this.enter.start,end:this.enter.end},r,i)},out(r=()=>{},i=()=>{}){Ut(t,e,{during:this.leave.during,start:this.leave.start,end:this.leave.end},r,i)}})}window.Element.prototype._x_toggleAndCascadeWithTransitions=function(t,e,n,r){const i=document.visibilityState==="visible"?requestAnimationFrame:setTimeout;let s=()=>i(n);if(e){t._x_transition&&(t._x_transition.enter||t._x_transition.leave)?t._x_transition.enter&&(Object.entries(t._x_transition.enter.during).length||Object.entries(t._x_transition.enter.start).length||Object.entries(t._x_transition.enter.end).length)?t._x_transition.in(n):s():t._x_transition?t._x_transition.in(n):s();return}t._x_hidePromise=t._x_transition?new Promise((o,a)=>{t._x_transition.out(()=>{},()=>o(r)),t._x_transitioning&&t._x_transitioning.beforeCancel(()=>a({isFromCancelledTransition:!0}))}):Promise.resolve(r),queueMicrotask(()=>{let o=pn(t);o?(o._x_hideChildren||(o._x_hideChildren=[]),o._x_hideChildren.push(t)):i(()=>{let a=c=>{let l=Promise.all([c._x_hidePromise,...(c._x_hideChildren||[]).map(a)]).then(([u])=>u==null?void 0:u());return delete c._x_hidePromise,delete c._x_hideChildren,l};a(t).catch(c=>{if(!c.isFromCancelledTransition)throw c})})})};function pn(t){let e=t.parentNode;if(e)return e._x_hidePromise?e:pn(e)}function Ut(t,e,{during:n,start:r,end:i}={},s=()=>{},o=()=>{}){if(t._x_transitioning&&t._x_transitioning.cancel(),Object.keys(n).length===0&&Object.keys(r).length===0&&Object.keys(i).length===0){s(),o();return}let a,c,l;Ur(t,{start(){a=e(t,r)},during(){c=e(t,n)},before:s,end(){a(),l=e(t,i)},after:o,cleanup(){c(),l()}})}function Ur(t,e){let n,r,i,s=Kt(()=>{g(()=>{n=!0,r||e.before(),i||(e.end(),Bt()),e.after(),t.isConnected&&e.cleanup(),delete t._x_transitioning})});t._x_transitioning={beforeCancels:[],beforeCancel(o){this.beforeCancels.push(o)},cancel:Kt(function(){for(;this.beforeCancels.length;)this.beforeCancels.shift()();s()}),finish:s},g(()=>{e.start(),e.during()}),kr(),requestAnimationFrame(()=>{if(n)return;let o=Number(getComputedStyle(t).transitionDuration.replace(/,.*/,"").replace("s",""))*1e3,a=Number(getComputedStyle(t).transitionDelay.replace(/,.*/,"").replace("s",""))*1e3;o===0&&(o=Number(getComputedStyle(t).animationDuration.replace("s",""))*1e3),g(()=>{e.before()}),r=!0,requestAnimationFrame(()=>{n||(g(()=>{e.end()}),Bt(),setTimeout(t._x_transitioning.finish,o+a),i=!0)})})}function rt(t,e,n){if(t.indexOf(e)===-1)return n;const r=t[t.indexOf(e)+1];if(!r||e==="scale"&&isNaN(r))return n;if(e==="duration"||e==="delay"){let i=r.match(/([0-9]+)ms/);if(i)return i[1]}return e==="origin"&&["top","right","left","center","bottom"].includes(t[t.indexOf(e)+2])?[r,t[t.indexOf(e)+2]].join(" "):r}var L=!1;function R(t,e=()=>{}){return(...n)=>L?e(...n):t(...n)}function Hr(t){return(...e)=>L&&t(...e)}var hn=[];function Et(t){hn.push(t)}function zr(t,e){hn.forEach(n=>n(t,e)),L=!0,_n(()=>{M(e,(n,r)=>{r(n,()=>{})})}),L=!1}var Ht=!1;function Wr(t,e){e._x_dataStack||(e._x_dataStack=t._x_dataStack),L=!0,Ht=!0,_n(()=>{Gr(e)}),L=!1,Ht=!1}function Gr(t){let e=!1;M(t,(r,i)=>{U(r,(s,o)=>{if(e&&Lr(s))return o();e=!0,i(s,o)})})}function _n(t){let e=z;ye((n,r)=>{let i=e(n);return X(i),()=>{}}),t(),ye(e)}function gn(t,e,n,r=[]){switch(t._x_bindings||(t._x_bindings=Y({})),t._x_bindings[e]=n,e=r.includes("camel")?ei(e):e,e){case"value":Jr(t,n);break;case"style":Yr(t,n);break;case"class":Vr(t,n);break;case"selected":case"checked":Xr(t,e,n);break;default:mn(t,e,n);break}}function Jr(t,e){if(xn(t))t.attributes.value===void 0&&(t.value=e),window.fromModel&&(typeof e=="boolean"?t.checked=yt(t.value)===e:t.checked=we(t.value,e));else if(le(t))Number.isInteger(e)?t.value=e:!Array.isArray(e)&&typeof e!="boolean"&&![null,void 0].includes(e)?t.value=String(e):Array.isArray(e)?t.checked=e.some(n=>we(n,t.value)):t.checked=!!e;else if(t.tagName==="SELECT")ti(t,e);else{if(t.value===e)return;t.value=e===void 0?"":e}}function Vr(t,e){t._x_undoAddedClasses&&t._x_undoAddedClasses(),t._x_undoAddedClasses=ce(t,e)}function Yr(t,e){t._x_undoAddedStyles&&t._x_undoAddedStyles(),t._x_undoAddedStyles=St(t,e)}function Xr(t,e,n){mn(t,e,n),Qr(t,e,n)}function mn(t,e,n){[null,void 0,!1].includes(n)&&ri(e)?t.removeAttribute(e):(yn(e)&&(n=e),Zr(t,e,n))}function Zr(t,e,n){t.getAttribute(e)!=n&&t.setAttribute(e,n)}function Qr(t,e,n){t[e]!==n&&(t[e]=n)}function ti(t,e){const n=[].concat(e).map(r=>r+"");Array.from(t.options).forEach(r=>{r.selected=n.includes(r.value)})}function ei(t){return t.toLowerCase().replace(/-(\w)/g,(e,n)=>n.toUpperCase())}function we(t,e){return t==e}function yt(t){return[1,"1","true","on","yes",!0].includes(t)?!0:[0,"0","false","off","no",!1].includes(t)?!1:t?!!t:null}var ni=new Set(["allowfullscreen","async","autofocus","autoplay","checked","controls","default","defer","disabled","formnovalidate","inert","ismap","itemscope","loop","multiple","muted","nomodule","novalidate","open","playsinline","readonly","required","reversed","selected","shadowrootclonable","shadowrootdelegatesfocus","shadowrootserializable"]);function yn(t){return ni.has(t)}function ri(t){return!["aria-pressed","aria-checked","aria-expanded","aria-selected"].includes(t)}function ii(t,e,n){return t._x_bindings&&t._x_bindings[e]!==void 0?t._x_bindings[e]:vn(t,e,n)}function si(t,e,n,r=!0){if(t._x_bindings&&t._x_bindings[e]!==void 0)return t._x_bindings[e];if(t._x_inlineBindings&&t._x_inlineBindings[e]!==void 0){let i=t._x_inlineBindings[e];return i.extract=r,We(()=>N(t,i.expression))}return vn(t,e,n)}function vn(t,e,n){let r=t.getAttribute(e);return r===null?typeof n=="function"?n():n:r===""?!0:yn(e)?!![e,"true"].includes(r):r}function le(t){return t.type==="checkbox"||t.localName==="ui-checkbox"||t.localName==="ui-switch"}function xn(t){return t.type==="radio"||t.localName==="ui-radio"}function wn(t,e){let n;return function(){const r=this,i=arguments,s=function(){n=null,t.apply(r,i)};clearTimeout(n),n=setTimeout(s,e)}}function bn(t,e){let n;return function(){let r=this,i=arguments;n||(t.apply(r,i),n=!0,setTimeout(()=>n=!1,e))}}function Sn({get:t,set:e},{get:n,set:r}){let i=!0,s,o=z(()=>{let a=t(),c=n();if(i)r(Mt(a)),i=!1;else{let l=JSON.stringify(a),u=JSON.stringify(c);l!==s?r(Mt(a)):l!==u&&e(Mt(c))}s=JSON.stringify(t()),JSON.stringify(n())});return()=>{X(o)}}function Mt(t){return typeof t=="object"?JSON.parse(JSON.stringify(t)):t}function oi(t){(Array.isArray(t)?t:[t]).forEach(n=>n(tt))}var q={},be=!1;function ai(t,e){if(be||(q=Y(q),be=!0),e===void 0)return q[t];q[t]=e,ee(q[t]),typeof e=="object"&&e!==null&&e.hasOwnProperty("init")&&typeof e.init=="function"&&q[t].init()}function ci(){return q}var En={};function li(t,e){let n=typeof e!="function"?()=>e:e;return t instanceof Element?Tn(t,n()):(En[t]=n,()=>{})}function ui(t){return Object.entries(En).forEach(([e,n])=>{Object.defineProperty(t,e,{get(){return(...r)=>n(...r)}})}),t}function Tn(t,e,n){let r=[];for(;r.length;)r.pop()();let i=Object.entries(e).map(([o,a])=>({name:o,value:a})),s=Ye(i);return i=i.map(o=>s.find(a=>a.name===o.name)?{name:`x-bind:${o.name}`,value:`"${o.value}"`}:o),re(t,i,n).map(o=>{r.push(o.runCleanups),o()}),()=>{for(;r.length;)r.pop()()}}var An={};function fi(t,e){An[t]=e}function di(t,e){return Object.entries(An).forEach(([n,r])=>{Object.defineProperty(t,n,{get(){return(...i)=>r.bind(e)(...i)},enumerable:!1})}),t}var pi={get reactive(){return Y},get release(){return X},get effect(){return z},get raw(){return Ie},get transaction(){return sr},version:"3.15.8",flushAndStopDeferringMutations:ur,dontAutoEvaluateFunctions:We,disableEffectScheduling:nr,startObservingMutations:Zt,stopObservingMutations:Be,setReactivityEngine:rr,onAttributeRemoved:Fe,onAttributesAdded:Ne,closestDataStack:B,skipDuringClone:R,onlyDuringClone:Hr,addRootSelector:ln,addInitSelector:un,setErrorHandler:gr,interceptClone:Et,addScopeToNode:ft,deferMutations:lr,mapAttributes:ie,evaluateLater:b,interceptInit:Ir,initInterceptors:ee,injectMagics:ct,setEvaluator:yr,setRawEvaluator:vr,mergeProxies:K,extractProp:si,findClosest:H,onElRemoved:Vt,closestRoot:bt,destroyTree:Q,interceptor:Ue,transition:Ut,setStyles:St,mutateDom:g,directive:v,entangle:Sn,throttle:bn,debounce:wn,evaluate:N,evaluateRaw:Sr,initTree:M,nextTick:ae,prefixed:Z,prefix:Tr,plugin:oi,magic:$,store:ai,start:Pr,clone:Wr,cloneNode:zr,bound:ii,$data:Ke,watch:Re,walk:U,data:fi,bind:li},tt=pi;function hi(t,e){const n=Object.create(null),r=t.split(",");for(let i=0;i<r.length;i++)n[r[i]]=!0;return i=>!!n[i]}var _i=Object.freeze({}),gi=Object.prototype.hasOwnProperty,Tt=(t,e)=>gi.call(t,e),F=Array.isArray,at=t=>On(t)==="[object Map]",mi=t=>typeof t=="string",ue=t=>typeof t=="symbol",At=t=>t!==null&&typeof t=="object",yi=Object.prototype.toString,On=t=>yi.call(t),$n=t=>On(t).slice(8,-1),fe=t=>mi(t)&&t!=="NaN"&&t[0]!=="-"&&""+parseInt(t,10)===t,vi=t=>{const e=Object.create(null);return n=>e[n]||(e[n]=t(n))},xi=vi(t=>t.charAt(0).toUpperCase()+t.slice(1)),Cn=(t,e)=>t!==e&&(t===t||e===e),zt=new WeakMap,it=[],C,D=Symbol("iterate"),Wt=Symbol("Map key iterate");function wi(t){return t&&t._isEffect===!0}function bi(t,e=_i){wi(t)&&(t=t.raw);const n=Ti(t,e);return e.lazy||n(),n}function Si(t){t.active&&(Mn(t),t.options.onStop&&t.options.onStop(),t.active=!1)}var Ei=0;function Ti(t,e){const n=function(){if(!n.active)return t();if(!it.includes(n)){Mn(n);try{return Oi(),it.push(n),C=n,t()}finally{it.pop(),Pn(),C=it[it.length-1]}}};return n.id=Ei++,n.allowRecurse=!!e.allowRecurse,n._isEffect=!0,n.active=!0,n.raw=t,n.deps=[],n.options=e,n}function Mn(t){const{deps:e}=t;if(e.length){for(let n=0;n<e.length;n++)e[n].delete(t);e.length=0}}var V=!0,de=[];function Ai(){de.push(V),V=!1}function Oi(){de.push(V),V=!0}function Pn(){const t=de.pop();V=t===void 0?!0:t}function O(t,e,n){if(!V||C===void 0)return;let r=zt.get(t);r||zt.set(t,r=new Map);let i=r.get(n);i||r.set(n,i=new Set),i.has(C)||(i.add(C),C.deps.push(i),C.options.onTrack&&C.options.onTrack({effect:C,target:t,type:e,key:n}))}function I(t,e,n,r,i,s){const o=zt.get(t);if(!o)return;const a=new Set,c=u=>{u&&u.forEach(p=>{(p!==C||p.allowRecurse)&&a.add(p)})};if(e==="clear")o.forEach(c);else if(n==="length"&&F(t))o.forEach((u,p)=>{(p==="length"||p>=r)&&c(u)});else switch(n!==void 0&&c(o.get(n)),e){case"add":F(t)?fe(n)&&c(o.get("length")):(c(o.get(D)),at(t)&&c(o.get(Wt)));break;case"delete":F(t)||(c(o.get(D)),at(t)&&c(o.get(Wt)));break;case"set":at(t)&&c(o.get(D));break}const l=u=>{u.options.onTrigger&&u.options.onTrigger({effect:u,target:t,key:n,type:e,newValue:r,oldValue:i,oldTarget:s}),u.options.scheduler?u.options.scheduler(u):u()};a.forEach(l)}var $i=hi("__proto__,__v_isRef,__isVue"),Ln=new Set(Object.getOwnPropertyNames(Symbol).map(t=>Symbol[t]).filter(ue)),Ci=In(),Mi=In(!0),Se=Pi();function Pi(){const t={};return["includes","indexOf","lastIndexOf"].forEach(e=>{t[e]=function(...n){const r=_(this);for(let s=0,o=this.length;s<o;s++)O(r,"get",s+"");const i=r[e](...n);return i===-1||i===!1?r[e](...n.map(_)):i}}),["push","pop","shift","unshift","splice"].forEach(e=>{t[e]=function(...n){Ai();const r=_(this)[e].apply(this,n);return Pn(),r}}),t}function In(t=!1,e=!1){return function(r,i,s){if(i==="__v_isReactive")return!t;if(i==="__v_isReadonly")return t;if(i==="__v_raw"&&s===(t?e?zi:jn:e?Hi:kn).get(r))return r;const o=F(r);if(!t&&o&&Tt(Se,i))return Reflect.get(Se,i,s);const a=Reflect.get(r,i,s);return(ue(i)?Ln.has(i):$i(i))||(t||O(r,"get",i),e)?a:Gt(a)?!o||!fe(i)?a.value:a:At(a)?t?Nn(a):ge(a):a}}var Li=Ii();function Ii(t=!1){return function(n,r,i,s){let o=n[r];if(!t&&(i=_(i),o=_(o),!F(n)&&Gt(o)&&!Gt(i)))return o.value=i,!0;const a=F(n)&&fe(r)?Number(r)<n.length:Tt(n,r),c=Reflect.set(n,r,i,s);return n===_(s)&&(a?Cn(i,o)&&I(n,"set",r,i,o):I(n,"add",r,i)),c}}function Ri(t,e){const n=Tt(t,e),r=t[e],i=Reflect.deleteProperty(t,e);return i&&n&&I(t,"delete",e,void 0,r),i}function qi(t,e){const n=Reflect.has(t,e);return(!ue(e)||!Ln.has(e))&&O(t,"has",e),n}function ki(t){return O(t,"iterate",F(t)?"length":D),Reflect.ownKeys(t)}var ji={get:Ci,set:Li,deleteProperty:Ri,has:qi,ownKeys:ki},Ni={get:Mi,set(t,e){return console.warn(`Set operation on key "${String(e)}" failed: target is readonly.`,t),!0},deleteProperty(t,e){return console.warn(`Delete operation on key "${String(e)}" failed: target is readonly.`,t),!0}},pe=t=>At(t)?ge(t):t,he=t=>At(t)?Nn(t):t,_e=t=>t,Ot=t=>Reflect.getPrototypeOf(t);function dt(t,e,n=!1,r=!1){t=t.__v_raw;const i=_(t),s=_(e);e!==s&&!n&&O(i,"get",e),!n&&O(i,"get",s);const{has:o}=Ot(i),a=r?_e:n?he:pe;if(o.call(i,e))return a(t.get(e));if(o.call(i,s))return a(t.get(s));t!==i&&t.get(e)}function pt(t,e=!1){const n=this.__v_raw,r=_(n),i=_(t);return t!==i&&!e&&O(r,"has",t),!e&&O(r,"has",i),t===i?n.has(t):n.has(t)||n.has(i)}function ht(t,e=!1){return t=t.__v_raw,!e&&O(_(t),"iterate",D),Reflect.get(t,"size",t)}function Ee(t){t=_(t);const e=_(this);return Ot(e).has.call(e,t)||(e.add(t),I(e,"add",t,t)),this}function Te(t,e){e=_(e);const n=_(this),{has:r,get:i}=Ot(n);let s=r.call(n,t);s?qn(n,r,t):(t=_(t),s=r.call(n,t));const o=i.call(n,t);return n.set(t,e),s?Cn(e,o)&&I(n,"set",t,e,o):I(n,"add",t,e),this}function Ae(t){const e=_(this),{has:n,get:r}=Ot(e);let i=n.call(e,t);i?qn(e,n,t):(t=_(t),i=n.call(e,t));const s=r?r.call(e,t):void 0,o=e.delete(t);return i&&I(e,"delete",t,void 0,s),o}function Oe(){const t=_(this),e=t.size!==0,n=at(t)?new Map(t):new Set(t),r=t.clear();return e&&I(t,"clear",void 0,void 0,n),r}function _t(t,e){return function(r,i){const s=this,o=s.__v_raw,a=_(o),c=e?_e:t?he:pe;return!t&&O(a,"iterate",D),o.forEach((l,u)=>r.call(i,c(l),c(u),s))}}function gt(t,e,n){return function(...r){const i=this.__v_raw,s=_(i),o=at(s),a=t==="entries"||t===Symbol.iterator&&o,c=t==="keys"&&o,l=i[t](...r),u=n?_e:e?he:pe;return!e&&O(s,"iterate",c?Wt:D),{next(){const{value:p,done:w}=l.next();return w?{value:p,done:w}:{value:a?[u(p[0]),u(p[1])]:u(p),done:w}},[Symbol.iterator](){return this}}}}function P(t){return function(...e){{const n=e[0]?`on key "${e[0]}" `:"";console.warn(`${xi(t)} operation ${n}failed: target is readonly.`,_(this))}return t==="delete"?!1:this}}function Fi(){const t={get(s){return dt(this,s)},get size(){return ht(this)},has:pt,add:Ee,set:Te,delete:Ae,clear:Oe,forEach:_t(!1,!1)},e={get(s){return dt(this,s,!1,!0)},get size(){return ht(this)},has:pt,add:Ee,set:Te,delete:Ae,clear:Oe,forEach:_t(!1,!0)},n={get(s){return dt(this,s,!0)},get size(){return ht(this,!0)},has(s){return pt.call(this,s,!0)},add:P("add"),set:P("set"),delete:P("delete"),clear:P("clear"),forEach:_t(!0,!1)},r={get(s){return dt(this,s,!0,!0)},get size(){return ht(this,!0)},has(s){return pt.call(this,s,!0)},add:P("add"),set:P("set"),delete:P("delete"),clear:P("clear"),forEach:_t(!0,!0)};return["keys","values","entries",Symbol.iterator].forEach(s=>{t[s]=gt(s,!1,!1),n[s]=gt(s,!0,!1),e[s]=gt(s,!1,!0),r[s]=gt(s,!0,!0)}),[t,n,e,r]}var[Di,Bi]=Fi();function Rn(t,e){const n=t?Bi:Di;return(r,i,s)=>i==="__v_isReactive"?!t:i==="__v_isReadonly"?t:i==="__v_raw"?r:Reflect.get(Tt(n,i)&&i in r?n:r,i,s)}var Ki={get:Rn(!1)},Ui={get:Rn(!0)};function qn(t,e,n){const r=_(n);if(r!==n&&e.call(t,r)){const i=$n(t);console.warn(`Reactive ${i} contains both the raw and reactive versions of the same object${i==="Map"?" as keys":""}, which can lead to inconsistencies. Avoid differentiating between the raw and reactive versions of an object and only use the reactive version if possible.`)}}var kn=new WeakMap,Hi=new WeakMap,jn=new WeakMap,zi=new WeakMap;function Wi(t){switch(t){case"Object":case"Array":return 1;case"Map":case"Set":case"WeakMap":case"WeakSet":return 2;default:return 0}}function Gi(t){return t.__v_skip||!Object.isExtensible(t)?0:Wi($n(t))}function ge(t){return t&&t.__v_isReadonly?t:Fn(t,!1,ji,Ki,kn)}function Nn(t){return Fn(t,!0,Ni,Ui,jn)}function Fn(t,e,n,r,i){if(!At(t))return console.warn(`value cannot be made reactive: ${String(t)}`),t;if(t.__v_raw&&!(e&&t.__v_isReactive))return t;const s=i.get(t);if(s)return s;const o=Gi(t);if(o===0)return t;const a=new Proxy(t,o===2?r:n);return i.set(t,a),a}function _(t){return t&&_(t.__v_raw)||t}function Gt(t){return!!(t&&t.__v_isRef===!0)}$("nextTick",()=>ae);$("dispatch",t=>ot.bind(ot,t));$("watch",(t,{evaluateLater:e,cleanup:n})=>(r,i)=>{let s=e(r),a=Re(()=>{let c;return s(l=>c=l),c},i);n(a)});$("store",ci);$("data",t=>Ke(t));$("root",t=>bt(t));$("refs",t=>(t._x_refs_proxy||(t._x_refs_proxy=K(Ji(t))),t._x_refs_proxy));function Ji(t){let e=[];return H(t,n=>{n._x_refs&&e.push(n._x_refs)}),e}var Pt={};function Dn(t){return Pt[t]||(Pt[t]=0),++Pt[t]}function Vi(t,e){return H(t,n=>{if(n._x_ids&&n._x_ids[e])return!0})}function Yi(t,e){t._x_ids||(t._x_ids={}),t._x_ids[e]||(t._x_ids[e]=Dn(e))}$("id",(t,{cleanup:e})=>(n,r=null)=>{let i=`${n}${r?`-${r}`:""}`;return Xi(t,i,e,()=>{let s=Vi(t,n),o=s?s._x_ids[n]:Dn(n);return r?`${n}-${o}-${r}`:`${n}-${o}`})});Et((t,e)=>{t._x_id&&(e._x_id=t._x_id)});function Xi(t,e,n,r){if(t._x_id||(t._x_id={}),t._x_id[e])return t._x_id[e];let i=r();return t._x_id[e]=i,n(()=>{delete t._x_id[e]}),i}$("el",t=>t);Bn("Focus","focus","focus");Bn("Persist","persist","persist");function Bn(t,e,n){$(e,r=>T(`You can't use [$${e}] without first installing the "${t}" plugin here: https://alpinejs.dev/plugins/${n}`,r))}v("modelable",(t,{expression:e},{effect:n,evaluateLater:r,cleanup:i})=>{let s=r(e),o=()=>{let u;return s(p=>u=p),u},a=r(`${e} = __placeholder`),c=u=>a(()=>{},{scope:{__placeholder:u}}),l=o();c(l),queueMicrotask(()=>{if(!t._x_model)return;t._x_removeModelListeners.default();let u=t._x_model.get,p=t._x_model.set,w=Sn({get(){return u()},set(S){p(S)}},{get(){return o()},set(S){c(S)}});i(w)})});v("teleport",(t,{modifiers:e,expression:n},{cleanup:r})=>{t.tagName.toLowerCase()!=="template"&&T("x-teleport can only be used on a <template> tag",t);let i=$e(n),s=t.content.cloneNode(!0).firstElementChild;t._x_teleport=s,s._x_teleportBack=t,t.setAttribute("data-teleport-template",!0),s.setAttribute("data-teleport-target",!0),t._x_forwardEvents&&t._x_forwardEvents.forEach(a=>{s.addEventListener(a,c=>{c.stopPropagation(),t.dispatchEvent(new c.constructor(c.type,c))})}),ft(s,{},t);let o=(a,c,l)=>{l.includes("prepend")?c.parentNode.insertBefore(a,c):l.includes("append")?c.parentNode.insertBefore(a,c.nextSibling):c.appendChild(a)};g(()=>{o(s,i,e),R(()=>{M(s)})()}),t._x_teleportPutBack=()=>{let a=$e(n);g(()=>{o(t._x_teleport,a,e)})},r(()=>g(()=>{s.remove(),Q(s)}))});var Zi=document.createElement("div");function $e(t){let e=R(()=>document.querySelector(t),()=>Zi)();return e||T(`Cannot find x-teleport element for selector: "${t}"`),e}var Kn=()=>{};Kn.inline=(t,{modifiers:e},{cleanup:n})=>{e.includes("self")?t._x_ignoreSelf=!0:t._x_ignore=!0,n(()=>{e.includes("self")?delete t._x_ignoreSelf:delete t._x_ignore})};v("ignore",Kn);v("effect",R((t,{expression:e},{effect:n})=>{n(b(t,e))}));function G(t,e,n,r){let i=t,s=c=>r(c),o={},a=(c,l)=>u=>l(c,u);if(n.includes("dot")&&(e=Qi(e)),n.includes("camel")&&(e=ts(e)),n.includes("passive")&&(o.passive=!0),n.includes("capture")&&(o.capture=!0),n.includes("window")&&(i=window),n.includes("document")&&(i=document),n.includes("debounce")){let c=n[n.indexOf("debounce")+1]||"invalid-wait",l=wt(c.split("ms")[0])?Number(c.split("ms")[0]):250;s=wn(s,l)}if(n.includes("throttle")){let c=n[n.indexOf("throttle")+1]||"invalid-wait",l=wt(c.split("ms")[0])?Number(c.split("ms")[0]):250;s=bn(s,l)}return n.includes("prevent")&&(s=a(s,(c,l)=>{l.preventDefault(),c(l)})),n.includes("stop")&&(s=a(s,(c,l)=>{l.stopPropagation(),c(l)})),n.includes("once")&&(s=a(s,(c,l)=>{c(l),i.removeEventListener(e,s,o)})),(n.includes("away")||n.includes("outside"))&&(i=document,s=a(s,(c,l)=>{t.contains(l.target)||l.target.isConnected!==!1&&(t.offsetWidth<1&&t.offsetHeight<1||t._x_isShown!==!1&&c(l))})),n.includes("self")&&(s=a(s,(c,l)=>{l.target===t&&c(l)})),e==="submit"&&(s=a(s,(c,l)=>{l.target._x_pendingModelUpdates&&l.target._x_pendingModelUpdates.forEach(u=>u()),c(l)})),(ns(e)||Un(e))&&(s=a(s,(c,l)=>{rs(l,n)||c(l)})),i.addEventListener(e,s,o),()=>{i.removeEventListener(e,s,o)}}function Qi(t){return t.replace(/-/g,".")}function ts(t){return t.toLowerCase().replace(/-(\w)/g,(e,n)=>n.toUpperCase())}function wt(t){return!Array.isArray(t)&&!isNaN(t)}function es(t){return[" ","_"].includes(t)?t:t.replace(/([a-z])([A-Z])/g,"$1-$2").replace(/[_\s]/,"-").toLowerCase()}function ns(t){return["keydown","keyup"].includes(t)}function Un(t){return["contextmenu","click","mouse"].some(e=>t.includes(e))}function rs(t,e){let n=e.filter(s=>!["window","document","prevent","stop","once","capture","self","away","outside","passive","preserve-scroll","blur","change","lazy"].includes(s));if(n.includes("debounce")){let s=n.indexOf("debounce");n.splice(s,wt((n[s+1]||"invalid-wait").split("ms")[0])?2:1)}if(n.includes("throttle")){let s=n.indexOf("throttle");n.splice(s,wt((n[s+1]||"invalid-wait").split("ms")[0])?2:1)}if(n.length===0||n.length===1&&Ce(t.key).includes(n[0]))return!1;const i=["ctrl","shift","alt","meta","cmd","super"].filter(s=>n.includes(s));return n=n.filter(s=>!i.includes(s)),!(i.length>0&&i.filter(o=>((o==="cmd"||o==="super")&&(o="meta"),t[`${o}Key`])).length===i.length&&(Un(t.type)||Ce(t.key).includes(n[0])))}function Ce(t){if(!t)return[];t=es(t);let e={ctrl:"control",slash:"/",space:" ",spacebar:" ",cmd:"meta",esc:"escape",up:"arrow-up",down:"arrow-down",left:"arrow-left",right:"arrow-right",period:".",comma:",",equal:"=",minus:"-",underscore:"_"};return e[t]=t,Object.keys(e).map(n=>{if(e[n]===t)return n}).filter(n=>n)}v("model",(t,{modifiers:e,expression:n},{effect:r,cleanup:i})=>{let s=t;e.includes("parent")&&(s=t.parentNode);let o=b(s,n),a;typeof n=="string"?a=b(s,`${n} = __placeholder`):typeof n=="function"&&typeof n()=="string"?a=b(s,`${n()} = __placeholder`):a=()=>{};let c=()=>{let h;return o(f=>h=f),Me(h)?h.get():h},l=h=>{let f;o(d=>f=d),Me(f)?f.set(h):a(()=>{},{scope:{__placeholder:h}})};typeof n=="string"&&t.type==="radio"&&g(()=>{t.hasAttribute("name")||t.setAttribute("name",n)});let u=e.includes("change")||e.includes("lazy"),p=e.includes("blur"),w=e.includes("enter"),S=u||p||w,A;if(L)A=()=>{};else if(S){let h=[],f=d=>l(mt(t,e,d,c()));if(u&&h.push(G(t,"change",e,f)),p&&(h.push(G(t,"blur",e,f)),t.form)){let d=()=>f({target:t});t.form._x_pendingModelUpdates||(t.form._x_pendingModelUpdates=[]),t.form._x_pendingModelUpdates.push(d),i(()=>t.form._x_pendingModelUpdates.splice(t.form._x_pendingModelUpdates.indexOf(d),1))}w&&h.push(G(t,"keydown",e,d=>{d.key==="Enter"&&f(d)})),A=()=>h.forEach(d=>d())}else{let h=t.tagName.toLowerCase()==="select"||["checkbox","radio"].includes(t.type)?"change":"input";A=G(t,h,e,f=>{l(mt(t,e,f,c()))})}if(e.includes("fill")&&([void 0,null,""].includes(c())||le(t)&&Array.isArray(c())||t.tagName.toLowerCase()==="select"&&t.multiple)&&l(mt(t,e,{target:t},c())),t._x_removeModelListeners||(t._x_removeModelListeners={}),t._x_removeModelListeners.default=A,i(()=>t._x_removeModelListeners.default()),t.form){let h=G(t.form,"reset",[],f=>{ae(()=>t._x_model&&t._x_model.set(mt(t,e,{target:t},c())))});i(()=>h())}t._x_model={get(){return c()},set(h){l(h)}},t._x_forceModelUpdate=h=>{h===void 0&&typeof n=="string"&&n.match(/\./)&&(h=""),window.fromModel=!0,g(()=>gn(t,"value",h)),delete window.fromModel},r(()=>{let h=c();e.includes("unintrusive")&&document.activeElement.isSameNode(t)||t._x_forceModelUpdate(h)})});function mt(t,e,n,r){return g(()=>{if(n instanceof CustomEvent&&n.detail!==void 0)return n.detail!==null&&n.detail!==void 0?n.detail:n.target.value;if(le(t))if(Array.isArray(r)){let i=null;return e.includes("number")?i=Lt(n.target.value):e.includes("boolean")?i=yt(n.target.value):i=n.target.value,n.target.checked?r.includes(i)?r:r.concat([i]):r.filter(s=>!is(s,i))}else return n.target.checked;else{if(t.tagName.toLowerCase()==="select"&&t.multiple)return e.includes("number")?Array.from(n.target.selectedOptions).map(i=>{let s=i.value||i.text;return Lt(s)}):e.includes("boolean")?Array.from(n.target.selectedOptions).map(i=>{let s=i.value||i.text;return yt(s)}):Array.from(n.target.selectedOptions).map(i=>i.value||i.text);{let i;return xn(t)?n.target.checked?i=n.target.value:i=r:i=n.target.value,e.includes("number")?Lt(i):e.includes("boolean")?yt(i):e.includes("trim")?i.trim():i}}})}function Lt(t){let e=t?parseFloat(t):null;return ss(e)?e:t}function is(t,e){return t==e}function ss(t){return!Array.isArray(t)&&!isNaN(t)}function Me(t){return t!==null&&typeof t=="object"&&typeof t.get=="function"&&typeof t.set=="function"}v("cloak",t=>queueMicrotask(()=>g(()=>t.removeAttribute(Z("cloak")))));un(()=>`[${Z("init")}]`);v("init",R((t,{expression:e},{evaluate:n})=>typeof e=="string"?!!e.trim()&&n(e,{},!1):n(e,{},!1)));v("text",(t,{expression:e},{effect:n,evaluateLater:r})=>{let i=r(e);n(()=>{i(s=>{g(()=>{t.textContent=s})})})});v("html",(t,{expression:e},{effect:n,evaluateLater:r})=>{let i=r(e);n(()=>{i(s=>{g(()=>{t.innerHTML=s,t._x_ignoreSelf=!0,M(t),delete t._x_ignoreSelf})})})});ie(Qe(":",tn(Z("bind:"))));var Hn=(t,{value:e,modifiers:n,expression:r,original:i},{effect:s,cleanup:o})=>{if(!e){let c={};ui(c),b(t,r)(u=>{Tn(t,u,i)},{scope:c});return}if(e==="key")return os(t,r);if(t._x_inlineBindings&&t._x_inlineBindings[e]&&t._x_inlineBindings[e].extract)return;let a=b(t,r);s(()=>a(c=>{c===void 0&&typeof r=="string"&&r.match(/\./)&&(c=""),g(()=>gn(t,e,c,n))})),o(()=>{t._x_undoAddedClasses&&t._x_undoAddedClasses(),t._x_undoAddedStyles&&t._x_undoAddedStyles()})};Hn.inline=(t,{value:e,modifiers:n,expression:r})=>{e&&(t._x_inlineBindings||(t._x_inlineBindings={}),t._x_inlineBindings[e]={expression:r,extract:!1})};v("bind",Hn);function os(t,e){t._x_keyExpression=e}ln(()=>`[${Z("data")}]`);v("data",(t,{expression:e},{cleanup:n})=>{if(as(t))return;e=e===""?"{}":e;let r={};ct(r,t);let i={};di(i,r);let s=N(t,e,{scope:i});(s===void 0||s===!0)&&(s={}),ct(s,t);let o=Y(s);ee(o);let a=ft(t,o);o.init&&N(t,o.init),n(()=>{o.destroy&&N(t,o.destroy),a()})});Et((t,e)=>{t._x_dataStack&&(e._x_dataStack=t._x_dataStack,e.setAttribute("data-has-alpine-state",!0))});function as(t){return L?Ht?!0:t.hasAttribute("data-has-alpine-state"):!1}v("show",(t,{modifiers:e,expression:n},{effect:r})=>{let i=b(t,n);t._x_doHide||(t._x_doHide=()=>{g(()=>{t.style.setProperty("display","none",e.includes("important")?"important":void 0)})}),t._x_doShow||(t._x_doShow=()=>{g(()=>{t.style.length===1&&t.style.display==="none"?t.removeAttribute("style"):t.style.removeProperty("display")})});let s=()=>{t._x_doHide(),t._x_isShown=!1},o=()=>{t._x_doShow(),t._x_isShown=!0},a=()=>setTimeout(o),c=Kt(p=>p?o():s(),p=>{typeof t._x_toggleAndCascadeWithTransitions=="function"?t._x_toggleAndCascadeWithTransitions(t,p,o,s):p?a():s()}),l,u=!0;r(()=>i(p=>{!u&&p===l||(e.includes("immediate")&&(p?a():s()),c(p),l=p,u=!1)}))});v("for",(t,{expression:e},{effect:n,cleanup:r})=>{let i=ls(e),s=b(t,i.items),o=b(t,t._x_keyExpression||"index");t._x_prevKeys=[],t._x_lookup={},n(()=>cs(t,i,s,o)),r(()=>{Object.values(t._x_lookup).forEach(a=>g(()=>{Q(a),a.remove()})),delete t._x_prevKeys,delete t._x_lookup})});function cs(t,e,n,r){let i=o=>typeof o=="object"&&!Array.isArray(o),s=t;n(o=>{us(o)&&o>=0&&(o=Array.from(Array(o).keys(),f=>f+1)),o===void 0&&(o=[]);let a=t._x_lookup,c=t._x_prevKeys,l=[],u=[];if(i(o))o=Object.entries(o).map(([f,d])=>{let m=Pe(e,d,f,o);r(x=>{u.includes(x)&&T("Duplicate key on x-for",t),u.push(x)},{scope:{index:f,...m}}),l.push(m)});else for(let f=0;f<o.length;f++){let d=Pe(e,o[f],f,o);r(m=>{u.includes(m)&&T("Duplicate key on x-for",t),u.push(m)},{scope:{index:f,...d}}),l.push(d)}let p=[],w=[],S=[],A=[];for(let f=0;f<c.length;f++){let d=c[f];u.indexOf(d)===-1&&S.push(d)}c=c.filter(f=>!S.includes(f));let h="template";for(let f=0;f<u.length;f++){let d=u[f],m=c.indexOf(d);if(m===-1)c.splice(f,0,d),p.push([h,f]);else if(m!==f){let x=c.splice(f,1)[0],E=c.splice(m-1,1)[0];c.splice(f,0,E),c.splice(m,0,x),w.push([x,E])}else A.push(d);h=d}for(let f=0;f<S.length;f++){let d=S[f];d in a&&(g(()=>{Q(a[d]),a[d].remove()}),delete a[d])}for(let f=0;f<w.length;f++){let[d,m]=w[f],x=a[d],E=a[m],W=document.createElement("div");g(()=>{E||T('x-for ":key" is undefined or invalid',s,m,a),E.after(W),x.after(E),E._x_currentIfEl&&E.after(E._x_currentIfEl),W.before(x),x._x_currentIfEl&&x.after(x._x_currentIfEl),W.remove()}),E._x_refreshXForScope(l[u.indexOf(m)])}for(let f=0;f<p.length;f++){let[d,m]=p[f],x=d==="template"?s:a[d];x._x_currentIfEl&&(x=x._x_currentIfEl);let E=l[m],W=u[m],et=document.importNode(s.content,!0).firstElementChild,me=Y(E);ft(et,me,s),et._x_refreshXForScope=Gn=>{Object.entries(Gn).forEach(([Jn,Vn])=>{me[Jn]=Vn})},g(()=>{x.after(et),R(()=>M(et))()}),typeof W=="object"&&T("x-for key cannot be an object, it must be a string or an integer",s),a[W]=et}for(let f=0;f<A.length;f++)a[A[f]]._x_refreshXForScope(l[u.indexOf(A[f])]);s._x_prevKeys=u})}function ls(t){let e=/,([^,\}\]]*)(?:,([^,\}\]]*))?$/,n=/^\s*\(|\)\s*$/g,r=/([\s\S]*?)\s+(?:in|of)\s+([\s\S]*)/,i=t.match(r);if(!i)return;let s={};s.items=i[2].trim();let o=i[1].replace(n,"").trim(),a=o.match(e);return a?(s.item=o.replace(e,"").trim(),s.index=a[1].trim(),a[2]&&(s.collection=a[2].trim())):s.item=o,s}function Pe(t,e,n,r){let i={};return/^\[.*\]$/.test(t.item)&&Array.isArray(e)?t.item.replace("[","").replace("]","").split(",").map(o=>o.trim()).forEach((o,a)=>{i[o]=e[a]}):/^\{.*\}$/.test(t.item)&&!Array.isArray(e)&&typeof e=="object"?t.item.replace("{","").replace("}","").split(",").map(o=>o.trim()).forEach(o=>{i[o]=e[o]}):i[t.item]=e,t.index&&(i[t.index]=n),t.collection&&(i[t.collection]=r),i}function us(t){return!Array.isArray(t)&&!isNaN(t)}function zn(){}zn.inline=(t,{expression:e},{cleanup:n})=>{let r=bt(t);r._x_refs||(r._x_refs={}),r._x_refs[e]=t,n(()=>delete r._x_refs[e])};v("ref",zn);v("if",(t,{expression:e},{effect:n,cleanup:r})=>{t.tagName.toLowerCase()!=="template"&&T("x-if can only be used on a <template> tag",t);let i=b(t,e),s=()=>{if(t._x_currentIfEl)return t._x_currentIfEl;let a=t.content.cloneNode(!0).firstElementChild;return ft(a,{},t),g(()=>{t.after(a),R(()=>M(a))()}),t._x_currentIfEl=a,t._x_undoIf=()=>{g(()=>{Q(a),a.remove()}),delete t._x_currentIfEl},a},o=()=>{t._x_undoIf&&(t._x_undoIf(),delete t._x_undoIf)};n(()=>i(a=>{a?s():o()})),r(()=>t._x_undoIf&&t._x_undoIf())});v("id",(t,{expression:e},{evaluate:n})=>{n(e).forEach(i=>Yi(t,i))});Et((t,e)=>{t._x_ids&&(e._x_ids=t._x_ids)});ie(Qe("@",tn(Z("on:"))));v("on",R((t,{value:e,modifiers:n,expression:r},{cleanup:i})=>{let s=r?b(t,r):()=>{};t.tagName.toLowerCase()==="template"&&(t._x_forwardEvents||(t._x_forwardEvents=[]),t._x_forwardEvents.includes(e)||t._x_forwardEvents.push(e));let o=G(t,e,n,a=>{s(()=>{},{scope:{$event:a},params:[a]})});i(()=>o())}));$t("Collapse","collapse","collapse");$t("Intersect","intersect","intersect");$t("Focus","trap","focus");$t("Mask","mask","mask");function $t(t,e,n){v(e,r=>T(`You can't use [x-${e}] without first installing the "${t}" plugin here: https://alpinejs.dev/plugins/${n}`,r))}tt.setEvaluator(Ve);tt.setRawEvaluator(Er);tt.setReactivityEngine({reactive:ge,effect:bi,release:Si,raw:_});var fs=tt,Wn=fs;const ds={top:"Top",bottom:"Bottom",intermediate:"Intermediate",tilt:"Tilt",blocking:"Blocking",overheated:"Overheated",timeout:"Timeout",start_moving_up:"Starting Up",start_moving_down:"Starting Down",moving_up:"Moving Up",moving_down:"Moving Down",stopped:"Stopped",top_tilt:"Top (Tilt)",bottom_tilt:"Bottom (Tilt)",unknown:"Unknown",on:"On",off:"Off"};async function y(t,e,n={}){const r=Object.keys(n).length?"?"+new URLSearchParams(n).toString():"",i=await fetch(e+r,{method:t});if(!i.ok){let o=`HTTP ${i.status}`;try{o=(await i.json()).error||o}catch{}throw new Error(o)}return(i.headers.get("content-type")||"").includes("application/json")?i.json():i.text()}function ps(t){if(!t)return"never";const e=Math.floor((Date.now()-t)/1e3);return e<5?"just now":e<60?`${e}s ago`:e<3600?`${Math.floor(e/60)}m ago`:`${Math.floor(e/3600)}h ago`}function hs(t){const e=Math.floor(t/1e3),n=Math.floor(e/3600)%24,r=Math.floor(e/60)%60,i=e%60;return`${String(n).padStart(2,"0")}:${String(r).padStart(2,"0")}:${String(i).padStart(2,"0")}`}function _s(t){if(!t)return"";const e=Math.floor(t/1e3),n=Math.floor(e/3600),r=Math.floor(e%3600/60),i=e%60;return n>0?`${n}h ${r}m ${i}s`:r>0?`${r}m ${i}s`:`${i}s`}function gs(t){return t>=-65?"▂▄█ "+t.toFixed(1)+" dBm":t>=-80?"▂▄░ "+t.toFixed(1)+" dBm":"▂░░ "+t.toFixed(1)+" dBm"}document.addEventListener("alpine:init",()=>{Wn.data("app",()=>({tab:"devices",deviceName:"",uptimeMs:0,get uptimeStr(){return _s(this.uptimeMs)},covers:[],lights:[],settingsOpen:null,scanning:!1,allDiscovered:[],get discoveredNew(){return this.allDiscovered.filter(t=>!t.already_configured&&!t.already_adopted)},get discoveredKnown(){return this.allDiscovered.filter(t=>t.already_configured||t.already_adopted)},adoptTarget:null,adoptName:"",adoptType:"cover",yamlContent:null,logCapture:!1,logLevel:"3",logAutoScroll:!0,logEntries:[],logLastTs:0,get filteredLog(){return this.logEntries.filter(t=>t.level<=parseInt(this.logLevel))},freq:{freq2:"",freq1:"",freq0:""},freqStatus:"",dumpActive:!1,dumpPackets:[],toast:{show:!1,error:!1,msg:""},_toastTimer:null,_pollCovers:null,_pollDisc:null,_pollLog:null,_pollDump:null,async init(){await this.refreshInfo(),await this.refreshCovers(),await this.loadFrequency(),this._pollCovers=setInterval(()=>this.refreshCovers(),3e3),this._pollDisc=setInterval(()=>this.refreshDiscovered(),3e3),this._pollDump=setInterval(()=>this.refreshDump(),2e3),this._pollLog=setInterval(()=>this.refreshLog(),1500)},showToast(t,e=!1){this._toastTimer&&clearTimeout(this._toastTimer),this.toast={show:!0,error:e,msg:t},this._toastTimer=setTimeout(()=>{this.toast.show=!1},3500)},async refreshInfo(){try{const t=await y("GET","/elero/api/info");this.deviceName=t.device_name||"",this.uptimeMs=t.uptime_ms||0,this.freq.freq2=t.freq2||this.freq.freq2,this.freq.freq1=t.freq1||this.freq.freq1,this.freq.freq0=t.freq0||this.freq.freq0}catch{}},async refreshCovers(){try{const t=await y("GET","/elero/api/configured");this.covers=(t.covers||[]).map(e=>({...e,_edit:{open_duration_ms:e.open_duration_ms,close_duration_ms:e.close_duration_ms,poll_interval_ms:e.poll_interval_ms}})),this.lights=(t.lights||[]).map(e=>({...e,_edit:{dim_duration_ms:e.dim_duration_ms}})),this.uptimeMs+=3e3}catch{}},toggleSettings(t){this.settingsOpen=this.settingsOpen===t.blind_address?null:t.blind_address},async coverCmd(t,e){try{await y("POST",`/elero/api/covers/${t.blind_address}/command`,{cmd:e}),this.showToast(`${t.name}: ${e} sent`)}catch(n){this.showToast(`Command failed: ${n.message}`,!0)}},async lightCmd(t,e){try{await y("POST",`/elero/api/lights/${t.blind_address}/command`,{cmd:e}),this.showToast(`${t.name}: ${e} sent`)}catch(n){this.showToast(`Command failed: ${n.message}`,!0)}},async saveSettings(t){try{await y("POST",`/elero/api/covers/${t.blind_address}/settings`,{open_duration:t._edit.open_duration_ms,close_duration:t._edit.close_duration_ms,poll_interval:t._edit.poll_interval_ms}),this.showToast(`${t.name}: settings saved`),this.settingsOpen=null,await this.refreshCovers()}catch(e){this.showToast(`Save failed: ${e.message}`,!0)}},async refreshDiscovered(){try{const t=await y("GET","/elero/api/discovered");this.scanning=t.scanning,this.allDiscovered=t.blinds||[]}catch{}},async startScan(){try{await y("POST","/elero/api/scan/start"),this.scanning=!0,this.showToast("Scan started")}catch(t){this.showToast(`Scan start failed: ${t.message}`,!0)}},async stopScan(){try{await y("POST","/elero/api/scan/stop"),this.scanning=!1,this.showToast("Scan stopped")}catch(t){this.showToast(`Scan stop failed: ${t.message}`,!0)}},startAdopt(t){this.adoptTarget=t,this.adoptName="",this.adoptType=t.last_state==="on"||t.last_state==="off"?"light":"cover"},async confirmAdopt(){if(this.adoptTarget)try{await y("POST",`/elero/api/discovered/${this.adoptTarget.blind_address}/adopt`,{name:this.adoptName||this.adoptTarget.blind_address,type:this.adoptType}),this.showToast(`Adopted as "${this.adoptName||this.adoptTarget.blind_address}"`),this.adoptTarget=null,this.tab="devices",await this.refreshCovers(),await this.refreshDiscovered()}catch(t){this.showToast(`Adopt failed: ${t.message}`,!0)}},showYamlBlind(t){t.last_state==="on"||t.last_state==="off"?this.yamlContent=`light:
  - platform: elero
    blind_address: ${t.blind_address}
    channel: ${t.channel}
    remote_address: ${t.remote_address}
    name: "My Light"
    # dim_duration: 0s
    hop: ${t.hop}
    payload_1: ${t.payload_1}
    payload_2: ${t.payload_2}
    pck_inf1: ${t.pck_inf1}
    pck_inf2: ${t.pck_inf2}
`:this.yamlContent=`cover:
  - platform: elero
    blind_address: ${t.blind_address}
    channel: ${t.channel}
    remote_address: ${t.remote_address}
    name: "My Blind"
    # open_duration: 25s
    # close_duration: 22s
    hop: ${t.hop}
    payload_1: ${t.payload_1}
    payload_2: ${t.payload_2}
    pck_inf1: ${t.pck_inf1}
    pck_inf2: ${t.pck_inf2}
`},async downloadYaml(){try{const t=await y("GET","/elero/api/yaml"),e=new Blob([t],{type:"text/plain"}),n=URL.createObjectURL(e),r=document.createElement("a");r.href=n,r.download="elero_blinds.yaml",r.click(),URL.revokeObjectURL(n)}catch(t){this.showToast(`YAML download failed: ${t.message}`,!0)}},copyYaml(){var t;(t=navigator.clipboard)==null||t.writeText(this.yamlContent).then(()=>this.showToast("Copied!")).catch(()=>this.showToast("Copy failed",!0))},async startCapture(){try{await y("POST","/elero/api/logs/capture/start"),this.logCapture=!0,this.showToast("Log capture started")}catch(t){this.showToast(`Failed: ${t.message}`,!0)}},async stopCapture(){try{await y("POST","/elero/api/logs/capture/stop"),this.logCapture=!1,this.showToast("Log capture stopped")}catch(t){this.showToast(`Failed: ${t.message}`,!0)}},async clearLog(){try{await y("POST","/elero/api/logs/clear"),this.logEntries=[],this.logLastTs=0,this.showToast("Log cleared")}catch(t){this.showToast(`Failed: ${t.message}`,!0)}},async refreshLog(){try{const t=await y("GET","/elero/api/logs",{since:this.logLastTs});if(this.logCapture=t.capture_active,t.entries&&t.entries.length>0){const e=t.entries.map((n,r)=>({...n,idx:this.logEntries.length+r}));this.logEntries.push(...e),this.logEntries.length>500&&this.logEntries.splice(0,this.logEntries.length-500),this.logLastTs=e[e.length-1].t,this.logAutoScroll&&this.$nextTick(()=>{const n=document.getElementById("log-box");n&&(n.scrollTop=n.scrollHeight)})}}catch{}},linkAddrs(t){if(!t)return"";const e={};for(const r of this.covers)e[r.blind_address]=r.name;for(const r of this.lights)e[r.blind_address]=r.name;return t.replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/0x[0-9a-fA-F]{6}/g,r=>{const i=e[r.toLowerCase()]||e[r];return i?`${r}<span class="blind-ref">(${i})</span>`:r})},async loadFrequency(){try{const t=await y("GET","/elero/api/frequency");this.freq={freq2:t.freq2,freq1:t.freq1,freq0:t.freq0}}catch{}},applyPreset(t){if(!t)return;const[e,n,r]=t.split(",");this.freq={freq2:"0x"+e,freq1:"0x"+n,freq0:"0x"+r}},async setFrequency(){this.freqStatus="Applying…";try{const t=await y("POST","/elero/api/frequency/set",this.freq);this.freq={freq2:t.freq2,freq1:t.freq1,freq0:t.freq0},this.freqStatus="",this.showToast(`Frequency set: ${t.freq2} ${t.freq1} ${t.freq0}`)}catch(t){this.freqStatus="",this.showToast(`Failed: ${t.message}`,!0)}},async startDump(){try{await y("POST","/elero/api/dump/start"),this.dumpActive=!0,this.showToast("Packet dump started")}catch(t){this.showToast(`Failed: ${t.message}`,!0)}},async stopDump(){try{await y("POST","/elero/api/dump/stop"),this.dumpActive=!1,this.showToast("Packet dump stopped")}catch(t){this.showToast(`Failed: ${t.message}`,!0)}},async clearDump(){try{await y("POST","/elero/api/packets/clear"),this.dumpPackets=[],this.showToast("Dump cleared")}catch(t){this.showToast(`Failed: ${t.message}`,!0)}},async downloadDump(){try{const t=await fetch("/elero/api/packets/download");if(!t.ok)throw new Error(`HTTP ${t.status}`);const e=await t.blob(),n=URL.createObjectURL(e),r=document.createElement("a");r.href=n,r.download=`elero_packets_${Date.now()}.json`,r.click(),URL.revokeObjectURL(n)}catch(t){this.showToast(`Download failed: ${t.message}`,!0)}},async refreshDump(){try{const t=await y("GET","/elero/api/packets");this.dumpActive=t.dump_active,this.dumpPackets=t.packets||[]}catch{}},stateLabel:t=>ds[t]||t||"Unknown",relTime:ps,fmtTs:hs,rssiIcon:gs}))});Wn.start();</script>
  <style rel="stylesheet" crossorigin>*,*:before,*:after{box-sizing:border-box;margin:0;padding:0}:root{--blue: #1a73e8;--blue-dk: #1557b0;--red: #d32f2f;--red-dk: #b71c1c;--green: #2e7d32;--amber: #e65100;--bg: #f0f2f5;--surface: #ffffff;--border: #e0e0e0;--muted: #757575;--text: #212121;--radius: 8px;--shadow: 0 1px 4px rgba(0,0,0,.12)}body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--text);line-height:1.5;font-size:14px}[x-cloak]{display:none!important}#app{max-width:760px;margin:0 auto;padding:12px 12px 40px}.header{display:flex;align-items:center;gap:10px;background:var(--blue);color:#fff;padding:14px 16px;border-radius:var(--radius);margin-bottom:10px}.header h1{font-size:1.1em;font-weight:600;flex:1}.header-right{font-size:.8em;opacity:.8}.uptime{font-size:.78em;opacity:.75}.tabs{display:flex;background:var(--surface);border-radius:var(--radius);box-shadow:var(--shadow);margin-bottom:12px;overflow:hidden}.tab-btn{flex:1;padding:10px 4px;border:none;background:none;font-size:.85em;font-weight:500;color:var(--muted);cursor:pointer;transition:color .15s,border-bottom .15s;border-bottom:3px solid transparent;display:flex;align-items:center;justify-content:center;gap:4px}.tab-btn.active{color:var(--blue);border-bottom-color:var(--blue)}.tab-btn:hover:not(.active){color:var(--text);background:#f8f9fa}.tab-badge{display:inline-flex;align-items:center;justify-content:center;background:var(--blue);color:#fff;border-radius:10px;font-size:.7em;min-width:18px;height:18px;padding:0 4px;font-weight:700}.tab-badge.log-badge{background:var(--amber)}.tab-content{min-height:200px}.card{background:var(--surface);border-radius:var(--radius);box-shadow:var(--shadow);margin-bottom:12px;overflow:hidden}.card-header{display:flex;justify-content:space-between;align-items:center;padding:10px 14px;background:#f8f9fa;border-bottom:1px solid var(--border);font-weight:600;font-size:.9em}.card-body{padding:14px}.btn{display:inline-flex;align-items:center;gap:4px;padding:7px 14px;border:none;border-radius:6px;font-size:.85em;font-weight:500;cursor:pointer;transition:background .15s;white-space:nowrap}.btn:disabled{opacity:.45;cursor:not-allowed}.btn-primary{background:var(--blue);color:#fff}.btn-primary:hover:not(:disabled){background:var(--blue-dk)}.btn-danger{background:var(--red);color:#fff}.btn-danger:hover:not(:disabled){background:var(--red-dk)}.btn-outline{background:#fff;color:var(--blue);border:1px solid var(--blue)}.btn-outline:hover:not(:disabled){background:#e8f0fe}.btn-open{background:#e8f5e9;color:var(--green);border:1px solid #a5d6a7}.btn-close{background:#fce4ec;color:var(--red);border:1px solid #f48fb1}.btn-stop{background:#f5f5f5;color:#424242;border:1px solid #bdbdbd}.btn-tilt{background:#e3f2fd;color:var(--blue);border:1px solid #90caf9}.btn-sm{padding:4px 10px;font-size:.8em}.btn-row{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}.cover-card{padding:14px}.cover-header{display:flex;justify-content:space-between;align-items:flex-start}.cover-name{font-weight:600;font-size:.95em}.cover-meta{font-size:.8em;color:var(--muted);margin-top:2px;display:flex;gap:8px;flex-wrap:wrap}.cover-right{display:flex;flex-direction:column;align-items:flex-end;gap:4px}.pos-bar{height:6px;background:#e0e0e0;border-radius:3px;overflow:hidden;margin:8px 0 2px}.pos-fill{height:100%;background:var(--blue);transition:width .4s}.pos-label{font-size:.8em;color:var(--muted)}.last-seen{font-size:.78em;color:var(--muted);margin-top:4px}.settings-btn{margin-left:auto}.settings-panel{margin-top:12px;border-top:1px solid var(--border);padding-top:10px}.settings-row{display:flex;align-items:center;gap:8px;margin-bottom:8px;font-size:.85em}.settings-row label{min-width:140px;color:var(--muted)}.settings-row input{border:1px solid #ccc;border-radius:6px;padding:5px 8px;font-size:.9em;width:130px}.settings-row input:focus{outline:none;border-color:var(--blue)}.settings-footer{display:flex;justify-content:flex-end;margin-top:4px}.state-badge{display:inline-block;padding:2px 8px;border-radius:4px;font-size:.76em;font-weight:600;white-space:nowrap}.state-top{background:#e8f5e9;color:var(--green)}.state-bottom{background:#fce4ec;color:var(--red)}.state-moving_up,.state-start_moving_up,.state-opening,.state-moving_down,.state-start_moving_down,.state-closing{background:#fff3e0;color:var(--amber)}.state-intermediate,.state-stopped{background:#e3f2fd;color:#1565c0}.state-idle,.state-unknown{background:#f5f5f5;color:var(--muted)}.state-blocking,.state-overheated,.state-timeout{background:#ffebee;color:var(--red)}.state-tilt,.state-top_tilt,.state-bottom_tilt{background:#f3e5f5;color:#6a1b9a}.adopted-tag{font-size:.72em;color:var(--blue);border:1px solid var(--blue);border-radius:4px;padding:1px 5px}.configured-tag{font-size:.76em;color:var(--green);font-weight:600}.disc-card{padding:12px 14px}.disc-card.known{opacity:.7}.disc-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:4px}.disc-meta{display:flex;flex-wrap:wrap;gap:10px;font-size:.82em;color:var(--muted)}.scan-status{display:flex;align-items:center;gap:6px;font-size:.85em}.status-dot{width:8px;height:8px;border-radius:50%;background:#9e9e9e}.status-dot.scanning{background:#4caf50;animation:pulse 1s infinite}@keyframes pulse{0%,to{opacity:1}50%{opacity:.3}}.hint{font-size:.8em;color:var(--muted);margin-top:8px}.log-controls{display:flex;align-items:center;gap:6px;flex-wrap:wrap;font-size:.85em}.level-select{border:1px solid #ccc;border-radius:6px;padding:4px 6px;font-size:.85em}.check-label{display:flex;align-items:center;gap:4px;font-size:.82em}.log-box{max-height:420px;overflow-y:auto;font-family:Menlo,Consolas,monospace;font-size:.78em;line-height:1.6;padding:8px;background:#1e1e2e;border-radius:6px;margin-top:10px}.log-line{display:flex;gap:8px;padding:1px 0}.log-ts{color:#888;min-width:60px}.log-level{min-width:48px;font-weight:700}.log-tag{color:#7dcfff;min-width:96px}.log-msg{color:#cdd6f4;word-break:break-all}.blind-ref{color:#a6e3a1;font-weight:700}.log-error .log-level{color:#f38ba8}.log-warn .log-level{color:#fab387}.log-info .log-level{color:#a6e3a1}.log-debug .log-level{color:#89b4fa}.log-verbose .log-level{color:#6c7086}.freq-row{margin-bottom:10px}.freq-preset{border:1px solid #ccc;border-radius:6px;padding:6px 8px;font-size:.85em}.freq-inputs{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:10px}.freq-field{display:flex;flex-direction:column;gap:2px}.freq-field label{font-size:.78em;color:var(--muted)}.freq-input{width:90px;border:1px solid #ccc;border-radius:6px;padding:6px 8px;font-size:.9em}.freq-input:focus{outline:none;border-color:var(--blue)}.badge{display:inline-block;padding:2px 8px;border-radius:12px;font-size:.75em;font-weight:600}.badge-active{background:#e8f5e9;color:var(--green)}.badge-idle{background:#f5f5f5;color:var(--muted)}.badge-count{background:#e3f2fd;color:#1565c0}.dump-wrap{overflow-x:auto;max-height:360px;overflow-y:auto;margin-top:10px}.pkt-table{width:100%;border-collapse:collapse;font-size:.78em}.pkt-table th{background:#f8f9fa;padding:5px 8px;text-align:left;border-bottom:2px solid var(--border);font-weight:600;white-space:nowrap}.pkt-table td{padding:4px 8px;border-bottom:1px solid #f0f0f0;vertical-align:top}.pkt-ok{background:#f1f8f4}.pkt-err{background:#fff5f5}.pkt-hex{font-family:monospace;font-size:.85em;word-break:break-all}.pkt-ok-badge{color:var(--green);font-weight:700}.pkt-err-badge{color:var(--red);font-weight:700}.info-grid{display:grid;grid-template-columns:130px 1fr;gap:6px 10px;font-size:.87em}.info-label{color:var(--muted);font-weight:500}.modal-overlay{display:none;position:fixed;top:0;right:0;bottom:0;left:0;background:#00000080;z-index:100;align-items:center;justify-content:center}.modal-overlay.active{display:flex}.modal{background:#fff;border-radius:12px;width:90%;max-width:520px;max-height:90vh;overflow-y:auto;box-shadow:0 8px 32px #0003}.modal-header{padding:14px 16px;border-bottom:1px solid var(--border);display:flex;justify-content:space-between;align-items:center}.modal-header h3{font-size:.95em}.modal-close{background:none;border:none;font-size:1.4em;cursor:pointer;color:var(--muted)}.modal-close:hover{color:var(--text)}.modal-body{padding:14px}.modal-footer{padding:10px 14px;border-top:1px solid var(--border);display:flex;justify-content:flex-end;gap:8px}.yaml-box{background:#263238;color:#e0e0e0;border-radius:6px;padding:12px;font-family:monospace;font-size:.8em;white-space:pre;overflow-x:auto;line-height:1.6;max-height:360px;overflow-y:auto}.toast{position:fixed;bottom:24px;left:50%;transform:translate(-50%);background:#333;color:#fff;padding:10px 20px;border-radius:8px;font-size:.88em;z-index:200;opacity:0;pointer-events:none;transition:opacity .25s;white-space:nowrap}.toast.show{opacity:1}.toast.error{background:var(--red)}.mono{font-family:monospace}.empty{text-align:center;padding:28px;color:var(--muted);font-size:.9em}</style>
</head>
<body>
<div id="app" x-data="app()" x-init="init()">

  <!-- Header -->
  <header class="header">
    <svg width="26" height="26" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true">
      <rect x="3" y="3" width="18" height="18" rx="2"/>
      <line x1="3" y1="9" x2="21" y2="9"/>
      <line x1="3" y1="15" x2="21" y2="15"/>
    </svg>
    <h1 x-text="deviceName || 'Elero Blind Manager'"></h1>
    <div class="header-right">
      <span class="uptime" x-text="uptimeStr"></span>
    </div>
  </header>

  <!-- Tab bar -->
  <nav class="tabs">
    <button class="tab-btn" :class="{active: tab==='devices'}"    @click="tab='devices'">
      Devices <span class="tab-badge" x-text="covers.length+lights.length" x-show="covers.length+lights.length>0"></span>
    </button>
    <button class="tab-btn" :class="{active: tab==='discovery'}"  @click="tab='discovery'">
      Discovery <span class="tab-badge" x-text="discoveredNew.length" x-show="discoveredNew.length>0"></span>
    </button>
    <button class="tab-btn" :class="{active: tab==='log'}"        @click="tab='log'">
      Log <span class="tab-badge log-badge" x-text="logEntries.length" x-show="logEntries.length>0"></span>
    </button>
    <button class="tab-btn" :class="{active: tab==='config'}"     @click="tab='config'">Configuration</button>
  </nav>

  <div class="tab-content">

    <!-- ══════════════ DEVICES TAB ══════════════ -->
    <section x-show="tab==='devices'" x-cloak>
      <template x-if="covers.length === 0 && lights.length === 0">
        <div class="empty">No devices configured. Add covers or lights in your ESPHome YAML.</div>
      </template>
      <template x-for="c in covers" :key="c.blind_address">
        <div class="card cover-card">
          <div class="cover-header">
            <div>
              <div class="cover-name" x-text="c.name"></div>
              <div class="cover-meta">
                <span class="mono" x-text="c.blind_address"></span>
                <span x-show="c.channel" x-text="'· CH '+c.channel"></span>
                <span x-show="c.rssi" :title="'RSSI: '+c.rssi.toFixed(1)+' dBm'" x-text="rssiIcon(c.rssi)"></span>
              </div>
            </div>
            <div class="cover-right">
              <span class="state-badge" :class="'state-'+c.last_state" x-text="stateLabel(c.last_state)"></span>
              <span class="adopted-tag" x-show="c.adopted">adopted</span>
            </div>
          </div>

          <!-- Position bar (only for covers with duration set) -->
          <template x-if="c.position !== null && c.open_duration_ms > 0">
            <div>
              <div class="pos-bar"><div class="pos-fill" :style="'width:'+Math.round(c.position*100)+'%'"></div></div>
              <div class="pos-label" x-text="Math.round(c.position*100)+'%'"></div>
            </div>
          </template>

          <!-- Last seen -->
          <div class="last-seen" x-text="'Last seen: '+relTime(c.last_seen_ms)"></div>

          <!-- Control buttons -->
          <div class="btn-row">
            <button class="btn btn-open"  @click="coverCmd(c, 'open')" title="Open">↑ Open</button>
            <button class="btn btn-stop"  @click="coverCmd(c, 'stop')" title="Stop">■ Stop</button>
            <button class="btn btn-close" @click="coverCmd(c, 'close')" title="Close">↓ Close</button>
            <button class="btn btn-tilt"  @click="coverCmd(c, 'tilt')" title="Tilt" x-show="c.supports_tilt">↔ Tilt</button>
            <button class="btn btn-outline settings-btn"
                    @click="toggleSettings(c)"
                    x-text="settingsOpen===c.blind_address ? '▲ Settings' : '▼ Settings'"></button>
          </div>

          <!-- Settings (collapsible) -->
          <div class="settings-panel" x-show="settingsOpen===c.blind_address" x-transition>
            <div class="settings-row">
              <label>Open duration (ms)</label>
              <input type="number" min="0" x-model.number="c._edit.open_duration_ms">
            </div>
            <div class="settings-row">
              <label>Close duration (ms)</label>
              <input type="number" min="0" x-model.number="c._edit.close_duration_ms">
            </div>
            <div class="settings-row">
              <label>Poll interval (ms)</label>
              <input type="number" min="0" x-model.number="c._edit.poll_interval_ms">
            </div>
            <div class="settings-footer">
              <button class="btn btn-primary" @click="saveSettings(c)">Save</button>
            </div>
          </div>
        </div>
      </template>

      <!-- Lights -->
      <template x-for="l in lights" :key="l.blind_address">
        <div class="card cover-card">
          <div class="cover-header">
            <div>
              <div class="cover-name" x-text="l.name"></div>
              <div class="cover-meta">
                <span class="mono" x-text="l.blind_address"></span>
                <span x-show="l.channel" x-text="'· CH '+l.channel"></span>
                <span x-show="l.rssi" :title="'RSSI: '+l.rssi.toFixed(1)+' dBm'" x-text="rssiIcon(l.rssi)"></span>
              </div>
            </div>
            <div class="cover-right">
              <span class="state-badge" :class="'state-'+l.last_state" x-text="stateLabel(l.last_state)"></span>
              <span class="adopted-tag" x-show="l.adopted">adopted</span>
            </div>
          </div>

          <!-- Brightness bar (only for dimmable lights) -->
          <template x-if="l.dim_duration_ms > 0">
            <div>
              <div class="pos-bar"><div class="pos-fill" :style="'width:'+Math.round(l.brightness*100)+'%'"></div></div>
              <div class="pos-label" x-text="Math.round(l.brightness*100)+'%'"></div>
            </div>
          </template>

          <!-- Last seen -->
          <div class="last-seen" x-text="'Last seen: '+relTime(l.last_seen_ms)"></div>

          <!-- Light control buttons -->
          <div class="btn-row">
            <button class="btn btn-open"  @click="lightCmd(l, 'on')" title="Turn On">On</button>
            <button class="btn btn-stop"  @click="lightCmd(l, 'stop')" title="Stop" x-show="l.dim_duration_ms > 0">Stop</button>
            <button class="btn btn-close" @click="lightCmd(l, 'off')" title="Turn Off">Off</button>
          </div>
        </div>
      </template>
    </section>

    <!-- ══════════════ DISCOVERY TAB ══════════════ -->
    <section x-show="tab==='discovery'" x-cloak>
      <div class="card">
        <div class="card-header">
          <span>RF Scan</span>
          <div class="scan-status">
            <span class="status-dot" :class="scanning ? 'scanning' : 'idle'"></span>
            <span x-text="scanning ? 'Scanning…' : 'Idle'"></span>
          </div>
        </div>
        <div class="card-body">
          <div class="btn-row">
            <button class="btn btn-primary" @click="startScan()" :disabled="scanning">▶ Start Scan</button>
            <button class="btn btn-danger"  @click="stopScan()"  :disabled="!scanning">■ Stop Scan</button>
          </div>
          <p class="hint">Operate your Elero remote during the scan to capture blinds.</p>
        </div>
      </div>

      <!-- New / unconfigured discovered blinds -->
      <template x-if="discoveredNew.length === 0 && !scanning">
        <div class="empty">No unconfigured blinds found. Start a scan and press your remote.</div>
      </template>
      <template x-for="b in discoveredNew" :key="b.blind_address">
        <div class="card disc-card">
          <div class="disc-header">
            <span class="mono" x-text="b.blind_address"></span>
            <span class="state-badge" :class="'state-'+b.last_state" x-text="stateLabel(b.last_state)"></span>
          </div>
          <div class="disc-meta">
            <span>CH <span x-text="b.channel"></span></span>
            <span>Remote <span class="mono" x-text="b.remote_address"></span></span>
            <span x-text="b.rssi.toFixed(1)+' dBm'"></span>
            <span x-text="b.times_seen+'× seen'"></span>
          </div>
          <div class="last-seen" x-text="'Last seen: '+relTime(b.last_seen_ms)"></div>
          <div class="btn-row">
            <button class="btn btn-outline" @click="showYamlBlind(b)">YAML…</button>
            <button class="btn btn-primary" @click="startAdopt(b)">Adopt → Devices</button>
          </div>
        </div>
      </template>

      <!-- Already configured / adopted blinds seen during scan -->
      <template x-for="b in discoveredKnown" :key="b.blind_address">
        <div class="card disc-card known">
          <div class="disc-header">
            <span class="mono" x-text="b.blind_address"></span>
            <span class="configured-tag" x-text="b.already_adopted ? '✓ Adopted' : '✓ Configured'"></span>
          </div>
          <div class="disc-meta">
            <span>CH <span x-text="b.channel"></span></span>
            <span x-text="b.rssi.toFixed(1)+' dBm'"></span>
            <span x-text="b.times_seen+'× seen'"></span>
            <span x-text="'Last seen: '+relTime(b.last_seen_ms)"></span>
          </div>
        </div>
      </template>

      <div style="text-align:center;margin-top:12px" x-show="allDiscovered.length>0">
        <button class="btn btn-outline" @click="downloadYaml()">↓ Download all YAML</button>
      </div>
    </section>

    <!-- ══════════════ LOG TAB ══════════════ -->
    <section x-show="tab==='log'" x-cloak>
      <div class="card">
        <div class="card-header">
          <div class="log-controls">
            <span class="status-dot" :class="logCapture ? 'scanning' : 'idle'"></span>
            <button class="btn btn-sm btn-primary" @click="startCapture()" :disabled="logCapture">Start</button>
            <button class="btn btn-sm btn-danger"  @click="stopCapture()"  :disabled="!logCapture">Stop</button>
            Level:
            <select x-model="logLevel" class="level-select">
              <option value="5">VERBOSE</option>
              <option value="4">DEBUG</option>
              <option value="3" selected>INFO</option>
              <option value="2">WARN</option>
              <option value="1">ERROR</option>
            </select>
          </div>
          <div>
            <label class="check-label">
              <input type="checkbox" x-model="logAutoScroll"> Auto-scroll
            </label>
            <button class="btn btn-sm btn-outline" @click="clearLog()">Clear</button>
          </div>
        </div>
        <div class="log-box" id="log-box">
          <template x-if="filteredLog.length === 0">
            <div class="empty">No log entries. <span x-show="!logCapture">Start capture first.</span></div>
          </template>
          <template x-for="e in filteredLog" :key="e.t+'_'+e.idx">
            <div class="log-line" :class="'log-'+e.level_str">
              <span class="log-ts" x-text="fmtTs(e.t)"></span>
              <span class="log-level" x-text="e.level_str.toUpperCase().padEnd(5)"></span>
              <span class="log-tag" x-text="e.tag"></span>
              <span class="log-msg" x-html="linkAddrs(e.msg)"></span>
            </div>
          </template>
        </div>
      </div>
    </section>

    <!-- ══════════════ CONFIGURATION TAB ══════════════ -->
    <section x-show="tab==='config'" x-cloak>

      <!-- Frequency -->
      <div class="card">
        <div class="card-header">CC1101 Frequency</div>
        <div class="card-body">
          <div class="freq-row">
            <select class="freq-preset" @change="applyPreset($event.target.value)">
              <option value="">— Preset —</option>
              <option value="21,71,7a">868.35 MHz (Standard Elero)</option>
              <option value="21,65,c0">868.95 MHz (Alternative)</option>
              <option value="10,A7,62">433.92 MHz</option>
            </select>
          </div>
          <div class="freq-inputs">
            <div class="freq-field"><label>freq2</label><input class="freq-input mono" x-model="freq.freq2" maxlength="4"></div>
            <div class="freq-field"><label>freq1</label><input class="freq-input mono" x-model="freq.freq1" maxlength="4"></div>
            <div class="freq-field"><label>freq0</label><input class="freq-input mono" x-model="freq.freq0" maxlength="4"></div>
          </div>
          <div class="btn-row">
            <button class="btn btn-primary" @click="setFrequency()">Apply</button>
            <span class="hint" x-text="freqStatus"></span>
          </div>
          <p class="hint">Hex values (e.g. 0x7a or 7a). Changes take effect immediately without reboot.</p>
        </div>
      </div>

      <!-- Packet Dump -->
      <div class="card">
        <div class="card-header">
          <span>Packet Dump</span>
          <div>
            <span class="badge" :class="dumpActive ? 'badge-active' : 'badge-idle'" x-text="dumpActive ? 'Active' : 'Idle'"></span>
            <span class="badge badge-count" x-text="dumpPackets.length" x-show="dumpPackets.length>0"></span>
          </div>
        </div>
        <div class="card-body">
          <div class="btn-row">
            <button class="btn btn-primary" @click="startDump()" :disabled="dumpActive">▶ Start Dump</button>
            <button class="btn btn-danger"  @click="stopDump()"  :disabled="!dumpActive">■ Stop</button>
            <button class="btn btn-outline" @click="clearDump()">Clear</button>
            <button class="btn btn-outline" @click="downloadDump()" :disabled="dumpPackets.length===0">&#8595; Download</button>
          </div>
          <p class="hint">Records all received RF packets (max 50). Green = valid, red = rejected.</p>
          <div class="dump-wrap" x-show="dumpPackets.length>0">
            <table class="pkt-table">
              <thead><tr><th>Time (ms)</th><th>Len</th><th>Status</th><th>Reason</th><th>Hex</th></tr></thead>
              <tbody>
                <template x-for="p in [...dumpPackets].reverse()" :key="p.t">
                  <tr :class="p.valid ? 'pkt-ok' : 'pkt-err'">
                    <td x-text="p.t"></td>
                    <td x-text="p.len"></td>
                    <td><span :class="p.valid ? 'pkt-ok-badge' : 'pkt-err-badge'" x-text="p.valid ? 'OK' : 'ERR'"></span></td>
                    <td x-text="p.reason||''"></td>
                    <td class="pkt-hex" x-text="p.hex"></td>
                  </tr>
                </template>
              </tbody>
            </table>
          </div>
        </div>
      </div>

      <!-- Hardware info -->
      <div class="card">
        <div class="card-header">Device Info</div>
        <div class="card-body info-grid">
          <span class="info-label">Device</span><span x-text="deviceName||'—'"></span>
          <span class="info-label">Uptime</span><span x-text="uptimeStr"></span>
          <span class="info-label">Frequency</span><span class="mono" x-text="freq.freq2+' / '+freq.freq1+' / '+freq.freq0"></span>
          <span class="info-label">Covers</span><span x-text="covers.filter(c=>!c.adopted).length+' configured, '+covers.filter(c=>c.adopted).length+' adopted'"></span>
          <span class="info-label">Lights</span><span x-text="lights.filter(l=>!l.adopted).length+' configured, '+lights.filter(l=>l.adopted).length+' adopted'"></span>
        </div>
      </div>
    </section>

  </div><!-- /tab-content -->

  <!-- Toast -->
  <div id="toast" class="toast" :class="{show: toast.show, error: toast.error}" x-text="toast.msg"></div>

  <!-- Adopt modal -->
  <div class="modal-overlay" :class="{active: adoptTarget}" @click.self="adoptTarget=null">
    <div class="modal" x-show="adoptTarget">
      <div class="modal-header">
        <h3>Adopt blind <span class="mono" x-text="adoptTarget?.blind_address"></span></h3>
        <button class="modal-close" @click="adoptTarget=null">&times;</button>
      </div>
      <div class="modal-body">
        <label class="settings-row">
          Friendly name
          <input type="text" x-model="adoptName" placeholder="e.g. Balcony" autofocus>
        </label>
        <label class="settings-row">
          Device type
          <select x-model="adoptType">
            <option value="cover">Cover (blind/shutter)</option>
            <option value="light">Light (dimmer)</option>
          </select>
        </label>
        <p class="hint" style="margin-top:8px">
          This device will appear in the Devices tab and can be controlled from the web UI.
          It is not a Home Assistant entity — reflash with the YAML below for permanent HA integration.
        </p>
      </div>
      <div class="modal-footer">
        <button class="btn btn-outline" @click="adoptTarget=null">Cancel</button>
        <button class="btn btn-primary" @click="confirmAdopt()">Adopt</button>
      </div>
    </div>
  </div>

  <!-- YAML modal -->
  <div class="modal-overlay" :class="{active: yamlContent}" @click.self="yamlContent=null">
    <div class="modal" x-show="yamlContent">
      <div class="modal-header">
        <h3>YAML Snippet</h3>
        <button class="modal-close" @click="yamlContent=null">&times;</button>
      </div>
      <div class="modal-body">
        <p class="hint" style="margin-bottom:8px">Add this to your ESPHome configuration for permanent HA integration:</p>
        <pre class="yaml-box" x-text="yamlContent"></pre>
      </div>
      <div class="modal-footer">
        <button class="btn btn-outline" @click="copyYaml()">Copy</button>
        <button class="btn btn-primary" @click="yamlContent=null">Close</button>
      </div>
    </div>
  </div>

</div><!-- /app -->
</body>
</html>
)rawliteral";

}  // namespace elero
}  // namespace esphome
