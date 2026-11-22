'use strict';
// WebKit's reply channel calls the same in-process C++ Session used by the pilot.
if(window.webkit?.messageHandlers?.inde){
  window.indeNativeRequest=async(path,options={})=>{
    const input=path==='/api/state'?{op:'view'}:path.startsWith('/api/image/')?{op:'image',resource:path.slice(11)}:JSON.parse(options.body||'{}');
    const r=await window.webkit.messageHandlers.inde.postMessage(input);
    if(path.startsWith('/api/image/')){if(!r.accepted)throw new Error(r.error);return new Response(Uint8Array.from(atob(r.data),c=>c.charCodeAt(0)),{headers:{'Content-Type':r.mime}});}
    return new Response(JSON.stringify(r),{headers:{'Content-Type':'application/json'}});
  };
  let lastDraft=null;
  window.indeNativeDraft=value=>{if(value===lastDraft)return;lastDraft=value;window.webkit.messageHandlers.inde.postMessage({op:'native_draft',value}).catch(()=>{});};
}
