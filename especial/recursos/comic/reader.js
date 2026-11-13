"use strict";
(() => {
  const comic = document.querySelector('#comic');
  let panels = [...comic.querySelectorAll('.comic-panel')];
  const mode = document.querySelector('#reading-mode');
  const media = matchMedia('(max-width: 600px)');
  let index = 0;

  function render() {
    panels = [...comic.querySelectorAll('.comic-panel')];
    index = Math.min(index, Math.max(0, panels.length - 1));
    const value = mode.value === 'auto' ? (media.matches ? 'mobile' : 'page') : mode.value;
    comic.className = 'comic mode-' + value + ' layout-' + (comic.dataset.layout||'cinema');
    comic.querySelectorAll('.scene').forEach(el=>{
      const wrapper=el.querySelector(':scope > .mobile-art');
      if(wrapper)wrapper.replaceWith(...wrapper.childNodes);
      const nodes=[...el.children];
      if(value==='mobile'){
        const art=document.createElement('div');art.className='mobile-art';
        nodes.filter(n=>n.matches('.art,.empty-art,.text,.sfx')).forEach(n=>art.append(n));
        el.prepend(art);
      }
    });
    panels.forEach((panel, i) => { panel.hidden = value === 'focus' && i !== index; });
    document.querySelector('#focus-nav').hidden = value !== 'focus';
    document.querySelector('#counter').textContent = `${index + 1} / ${panels.length}`;
    document.querySelector('#previous').disabled = index === 0;
    document.querySelector('#next').disabled = index === panels.length - 1;
  }
  mode.addEventListener('change', render);
  media.addEventListener('change', render);
  document.querySelector('#previous').onclick = () => { index = Math.max(0, index - 1); render(); };
  document.querySelector('#next').onclick = () => { index = Math.min(panels.length - 1, index + 1); render(); };
  document.addEventListener('keydown', e => {
    if (mode.value !== 'focus' || e.target.closest('select,input,textarea,.bubble-scroll,button')) return;
    if (e.key === 'ArrowRight' && index < panels.length - 1) { index++; render(); }
    if (e.key === 'ArrowLeft' && index > 0) { index--; render(); }
  });
  document.querySelector('#motion').onclick = e => {
    const paused = document.body.classList.toggle('motion-paused');
    e.currentTarget.textContent = paused ? 'Ativar animações' : 'Pausar animações';
    e.currentTarget.setAttribute('aria-pressed', String(paused));
  };
  document.querySelector('#transcript-toggle').onclick = e => {
    const transcript = document.querySelector('#transcript');
    transcript.hidden = !transcript.hidden;
    e.currentTarget.setAttribute('aria-expanded', String(!transcript.hidden));
  };
  const updateOverflow=()=>{
    document.querySelectorAll('.balloon').forEach(el=>{const scroll=el.querySelector('.bubble-scroll'),hint=el.querySelector('.overflow-hint');if(hint)hint.hidden=scroll.scrollHeight<=scroll.clientHeight+1;});
  };
  let pending=false;
  const scheduleOverflow=()=>{if(pending)return;pending=true;requestAnimationFrame(()=>{pending=false;updateOverflow();});};
  const observer=new MutationObserver(scheduleOverflow);observer.observe(comic,{childList:true,subtree:true,characterData:true});
  const resize=new ResizeObserver(scheduleOverflow);comic.querySelectorAll('.bubble-scroll').forEach(el=>resize.observe(el));
  document.fonts.addEventListener('loadingdone',scheduleOverflow);
  document.querySelector('#font-profile').onchange=e=>{document.body.dataset.fontProfile=e.target.value;scheduleOverflow();};
  document.querySelector('#gloss-toggle').onclick=e=>{const hidden=document.body.classList.toggle('hide-glosses');e.currentTarget.setAttribute('aria-pressed',String(!hidden));scheduleOverflow();};
  render();scheduleOverflow();
})();
