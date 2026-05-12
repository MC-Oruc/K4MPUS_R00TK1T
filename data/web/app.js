// Uptime ticker
(function(){
  const st=Date.now();
  function tick(){
    const s=Math.floor((Date.now()-st)/1000);
    const m=Math.floor(s/60);
    const h=Math.floor(m/60);
    const el=document.getElementById('upt');
    if(el) el.textContent=h+':'+(m%60).toString().padStart(2,'0')+':'+(s%60).toString().padStart(2,'0');
    requestAnimationFrame(tick);
  }
  tick();
})();

// Responsive ASCII-art fitter: scale monospace font so longest line fits header width (no horizontal scroll)
(function(){
  const pre = document.querySelector('header#hdrName pre');
  if(!pre) return;
  const container = pre.parentElement || pre;
  pre.style.overflowX = 'hidden';
  function measureCharWidth(){
    const cs = getComputedStyle(pre);
    const probe = document.createElement('span');
    probe.style.position = 'absolute';
    probe.style.left = '-99999px';
    probe.style.top = '-99999px';
    probe.style.visibility = 'hidden';
    probe.style.whiteSpace = 'nowrap';
    probe.style.fontFamily = cs.fontFamily || 'monospace';
    probe.style.fontWeight = cs.fontWeight || 'normal';
    probe.style.fontStyle = cs.fontStyle || 'normal';
    if(cs.fontStretch) probe.style.fontStretch = cs.fontStretch;
    probe.style.letterSpacing = cs.letterSpacing || '0px';
    probe.style.fontSize = '100px';
    const N = 20; probe.textContent = '0'.repeat(N);
    document.body.appendChild(probe);
    const charW = probe.getBoundingClientRect().width / N;
    document.body.removeChild(probe);
    return Math.max(0.1, charW / 100); // px-per-char per 1px font-size (guard against zero)
  }
  function fitAscii(){
    const text = pre.textContent || '';
    if(!text) return;
    const lines = text.split('\n');
    let maxLen = 0; for(const l of lines){ if(l.length > maxLen) maxLen = l.length; }
    if(maxLen <= 0) return;
    const cw = Math.max(20, (container.clientWidth || window.innerWidth) - 8); // safety margin
    const charPerPx = measureCharWidth();
    let fontPx = Math.floor((cw - 2) / (maxLen * charPerPx));
    // allow very small if needed to avoid scroll; JS will ensure no overflow
    if(fontPx < 3) fontPx = 3;
    pre.style.fontSize = fontPx + 'px';
    // final safety: if still overflowing, decrement until it fits or reach min
    let guard = 200;
    while(pre.scrollWidth > container.clientWidth && fontPx > 3 && guard-- > 0){
      fontPx -= 1;
      pre.style.fontSize = fontPx + 'px';
    }
  }
  const debounce = (fn, ms)=>{ let to; return ()=>{ clearTimeout(to); to=setTimeout(fn, ms); }; };
  const fitDebounced = debounce(fitAscii, 120);
  window.addEventListener('resize', fitDebounced);
  window.addEventListener('orientationchange', fitAscii);
  if('ResizeObserver' in window){
    const ro = new ResizeObserver(fitDebounced);
    ro.observe(container);
  }
  // run after short delay (allow layout)
  setTimeout(()=>{ requestAnimationFrame(fitAscii); }, 60);
})();

// Company input sanitizer
(function(){
  const c=document.querySelector('input[name=company]');
  if(c){ c.addEventListener('input',e=>{ e.target.value=e.target.value.toUpperCase().replace(/[^0-9A-F]/g,'').slice(0,4); }); }
})();

// Form save handler
(function(){
  const form=document.querySelector('form');
  const resBox=document.getElementById('result');
  const modeLabel=document.getElementById('modeVal');
  if(!form) return;
  form.addEventListener('submit',e=>{
    e.preventDefault();
    const btn=form.querySelector('button[type=submit]');
    btn.disabled=true; btn.textContent='Kaydediliyor...';
    const fd=new FormData(form); const p=new URLSearchParams(); fd.forEach((v,k)=>p.append(k,v));
    fetch('/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:p.toString()})
    .then(r=>r.text()).then(t=>{
      resBox.style.display='block';
      if(t==='OK'){
        resBox.style.background='#1d4027'; resBox.style.borderColor='#255c33'; resBox.style.color='#8df59f';
        resBox.textContent='Kaydedildi'; modeLabel.textContent=form.elements['mode'].value;
      } else if(t==='NO CHANGE'){
        resBox.style.background='#3a3423'; resBox.style.borderColor='#6a5d2a'; resBox.style.color='#e5d48a';
        resBox.textContent='Degisiklik yok';
      } else {
        resBox.style.background='#442226'; resBox.style.borderColor='#6a2a30'; resBox.style.color='#ffb2b9';
        resBox.textContent='Hata / Beklenmeyen yanit: '+t;
      }
      setTimeout(()=>{btn.disabled=false; btn.textContent='Kaydet';},400);
    })
    .catch(()=>{
      resBox.style.display='block'; resBox.style.background='#442226'; resBox.style.borderColor='#6a2a30'; resBox.style.color='#ffb2b9';
      resBox.textContent='Baglanti hatasi'; btn.disabled=false; btn.textContent='Kaydet';
    });
  });
})();

// Initial data load
(function(){
  // Fill config
  fetch('/config.json').then(r=>r.json()).then(j=>{
    document.getElementById('modeVal').textContent = j.operatingMode || '';
    document.getElementById('modeSel').value = j.operatingMode || 'wifi';
    document.getElementById('staSsid').value = j.staSsid || '';
    document.getElementById('staPass').value = j.staPass || '';
    document.getElementById('devName').value = j.deviceName || '';
  if(document.getElementById('apSsid')) document.getElementById('apSsid').value = j.apSsid || '';
  if(document.getElementById('apPass')) document.getElementById('apPass').value = j.apPass || '';
    document.getElementById('companyInput').value = j.companyId || '';
    // LEDs
    document.getElementById('led_wifi').value = j.led.wifi.mode;
    document.getElementById('speed_wifi').value = j.led.wifi.speed;
    document.getElementById('led_ble').value = j.led.ble.mode;
    document.getElementById('speed_ble').value = j.led.ble.speed;
    document.getElementById('led_dual').value = j.led.dual.mode;
    document.getElementById('speed_dual').value = j.led.dual.speed;
    document.getElementById('led_dualsta').value = j.led.dualsta.mode;
    document.getElementById('speed_dualsta').value = j.led.dualsta.speed;
    document.getElementById('led_wifi_conn').value = j.wifiState.conn.mode;
    document.getElementById('speed_wifi_conn').value = j.wifiState.conn.speed;
    document.getElementById('led_wifi_ok').value = j.wifiState.ok.mode;
    document.getElementById('speed_wifi_ok').value = j.wifiState.ok.speed;
    document.getElementById('led_wifi_err').value = j.wifiState.err.mode;
    document.getElementById('speed_wifi_err').value = j.wifiState.err.speed;
  }).catch(()=>{});
  // Fill company select
  fetch('/company_options').then(r=>r.text()).then(html=>{
    const sel=document.getElementById('companySelect');
    if(sel) {
      sel.innerHTML = "<option value=''>-- Liste Seç (veya elle yaz) --</option>" + html;
      // If an ID is already in the input (from config or active_label), select it in the list
      try{
        const inp = document.getElementById('companyInput');
        const cur = inp && inp.value ? inp.value.trim().toUpperCase() : '';
        if(cur){
          for(let i=0;i<sel.options.length;i++){
            if(sel.options[i].value.toUpperCase()===cur){ sel.selectedIndex = i; /* trigger change so listeners can react */ sel.dispatchEvent(new Event('change')); break; }
          }
        }
      }catch(e){}
    }
  }).catch(()=>{});
  // Build schedule rows for visual quick-select
  fetch('/schedule_data').then(r=>r.text()).then(t=>{
    const tbody=document.getElementById('schedTbody'); if(!tbody) return;
    const lines=t.trim().split(/\n+/);
    const tr = (cells)=>`<tr class='schedRow' data-label='${cells[4]||''}' style='cursor:pointer;border-top:1px solid #333'><td style='padding:4px'>${dowName(parseInt(cells[0]))}</td><td style='padding:4px'>${cells[1]}-${cells[2]}</td><td style='padding:4px'>${cells[3]}</td><td style='padding:4px'>${cells[4]||''}</td></tr>`;
    tbody.innerHTML = lines.filter(l=>l.trim()).map(line=>tr(line.split(','))).join('');
  }).catch(()=>{});
  // Active label highlight
  fetch('/active_label').then(r=>r.json()).then(o=>{
    if(o){
      if(o.label){
        document.querySelectorAll('.schedRow').forEach(r=>{
          if(r.getAttribute('data-label') && r.getAttribute('data-label').toLowerCase()===o.label.toLowerCase()) r.classList.add('activeRow');
        });
      }
      // If server provides an active company id, populate the company input/select
      try{
        const inp = document.getElementById('companyInput');
        const sel = document.getElementById('companySelect');
        if(o.id && inp){ inp.value = o.id; inp.dispatchEvent(new Event('input')); }
        if(o.id && sel){
          for(let i=0;i<sel.options.length;i++){
            if(sel.options[i].value.toUpperCase()===o.id.toUpperCase()){ sel.selectedIndex = i; break; }
          }
        }
        if(o.mode){ const modeLabel = document.getElementById('modeVal'); if(modeLabel) modeLabel.textContent = o.mode; }
      }catch(e){}
    }
  }).catch(()=>{});
  function dowName(d){ return ['?','Pazartesi','Salı','Çarşamba','Perşembe','Cuma','Cumartesi','Pazar'][d]||'?'; }
})();

// Device time viewer and setter (always display/set Turkey time correctly; avoid double TZ application)
(function(){
  const timeBox = document.getElementById('deviceTime');
  const input = document.getElementById('dtLocal');
  const btn = document.getElementById('btnTimeSet');
  const msg = document.getElementById('timeMsg');
  if(!timeBox) return;

  function pad(n){ return (n<10?'0':'')+n; }
  // Format UTC epoch seconds as UTC string
  function fmtUTC(utcSec){
    const d = new Date(utcSec*1000);
    return `${d.getUTCFullYear()}-${pad(d.getUTCMonth()+1)}-${pad(d.getUTCDate())} ${pad(d.getUTCHours())}:${pad(d.getUTCMinutes())}:${pad(d.getUTCSeconds())}`;
  }
  // Format TR local using device tz (minutes) but independent from viewer TZ
  function fmtTR(utcSec, tzMin){
    const ms = (utcSec*1000) + (tzMin*60*1000);
    const d = new Date(ms);
    // Use UTC getters on shifted time -> yields TR wall-clock
    return `${d.getUTCFullYear()}-${pad(d.getUTCMonth()+1)}-${pad(d.getUTCDate())} ${pad(d.getUTCHours())}:${pad(d.getUTCMinutes())}:${pad(d.getUTCSeconds())}`;
  }
  // Build datetime-local value (YYYY-MM-DDTHH:MM) for TR wall-clock
  function isoLocalFromUtcAndTz(utcSec, tzMin){
    const ms = (utcSec*1000) + (tzMin*60*1000);
    const d = new Date(ms);
    return `${d.getUTCFullYear()}-${pad(d.getUTCMonth()+1)}-${pad(d.getUTCDate())}T${pad(d.getUTCHours())}:${pad(d.getUTCMinutes())}`;
  }
  // Parse datetime-local string as TR time and convert to UTC epoch seconds
  function parseLocalTRToUtcEpoch(v, tzMin){
    // v: YYYY-MM-DDTHH:MM (no timezone). Interpret as TR wall-clock then subtract tz.
    const m = /^([0-9]{4})-([0-9]{2})-([0-9]{2})T([0-9]{2}):([0-9]{2})$/.exec(v);
    if(!m) return NaN;
    const y = parseInt(m[1],10), mo = parseInt(m[2],10)-1, da = parseInt(m[3],10);
    const h = parseInt(m[4],10), mi = parseInt(m[5],10);
    const msLocal = Date.UTC(y, mo, da, h, mi, 0);
    const msUtc = msLocal - (tzMin*60*1000);
    return Math.floor(msUtc/1000);
  }

  let first = true;
  function pollNow(){
    fetch('/now').then(r=>r.json()).then(o=>{
      if(!o || o.ok===false){ timeBox.textContent = 'Cihaz saati alınamadı'; return; }
      const utc = o.utc_epoch|0; const q = o.quality|0; const stale = !!o.stale; const tz = o.tz|0;
      const trStr = fmtTR(utc, tz);
      const utcStr = fmtUTC(utc);
      timeBox.textContent = `UTC: ${utcStr} | Türkiye: ${trStr} (UTC${tz>=0?'+':''}${(tz/60)|0}) | kalite=${q}${stale?' • stale':''}`;
      if(first && input){ input.value = isoLocalFromUtcAndTz(utc, tz); first = false; }
    }).catch(()=>{}).finally(()=>{ setTimeout(pollNow, 1000); });
  }
  pollNow();

  if(btn && input){
    btn.addEventListener('click', ()=>{
      const v = (input.value||'').trim();
      if(!v){ msg.textContent = 'Tarih/saat seçin'; msg.style.color = '#f88'; return; }
      const tz = 180; // Türkiye saati (UTC+3)
      const epoch = parseLocalTRToUtcEpoch(v, tz);
      if(!isFinite(epoch)){ msg.textContent = 'Geçersiz tarih/saat'; msg.style.color = '#f88'; return; }
      msg.textContent = 'Gönderiliyor...'; msg.style.color = '#888';
      fetch('/time', { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:`epoch=${encodeURIComponent(epoch)}&tz=${encodeURIComponent(tz)}` })
        .then(r=>r.text()).then(t=>{
          if(t==='OK'){ msg.textContent = 'Ayarlandı'; msg.style.color = '#8df59f'; first = true; }
          else { msg.textContent = 'Hata: '+t; msg.style.color = '#f88'; }
        })
        .catch(()=>{ msg.textContent = 'Bağlantı hatası'; msg.style.color = '#f88'; });
    });
  }
})();

// Tabs
(function(){
  const tabs=document.querySelectorAll('.tab-btn');
  const panels=document.querySelectorAll('.tab-panel');
  if(!(tabs.length && panels.length)) return;
  tabs.forEach(b=>b.addEventListener('click',()=>{
    tabs.forEach(x=>x.classList.remove('active'));
    b.classList.add('active');
    const t=b.dataset.tab;
    panels.forEach(p=>{ p.classList.toggle('active', p.id===('tab-'+t)); });
  }));
})();

// Yerleşik düzenleme: seçili girdiyi düzenle (isim, HEX, kategori)
(function(){
  const sel=document.getElementById('companySelect'); if(!sel) return;
  const parent=sel.parentElement; const btn=document.createElement('button');
  btn.type='button'; btn.textContent='Düzenle'; btn.style.marginLeft='6px'; parent.appendChild(btn);
  btn.addEventListener('click',()=>{
    const o=sel.options[sel.selectedIndex]; if(!o) { alert('Düzenlenecek öğe seçin'); return; }
    const curName = o.dataset.name || '';
    const curCat = o.dataset.cat || '';
    const curId = o.value || '';
    const nvName = prompt('Yeni ad', curName); if(nvName===null) return;
    const nvId = prompt('Yeni HEX', curId); if(nvId===null) return;
    const nvCat = prompt('Kategori (L/D/Lab/Amfi/Amfi Derslik)', curCat); if(nvCat===null) return;
    const hex = nvId.toUpperCase().trim();
    if(!/^([0-9A-F]{4,})$/.test(hex)){ alert('Geçersiz HEX'); return; }
    fetch('/cid_update',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'old_id='+encodeURIComponent(curId)+'&id='+encodeURIComponent(hex)+'&name='+encodeURIComponent(nvName)+'&cat='+encodeURIComponent(nvCat)})
      .then(r=>r.json()).then(j=>{ if(j.status==='OK') location.reload(); else alert('Hata: '+j.status); })
      .catch(()=>alert('Ağ hatası'));
  });
})();

// Add custom company id UI (persists to single company file)
(function(){
  const companyFieldset=document.querySelector('fieldset'); if(!companyFieldset) return;
  const addBox=document.createElement('div'); addBox.style.marginTop='10px';
  addBox.innerHTML="<details><summary>Yeni Company ID Ekle</summary><div style='margin-top:8px;display:flex;flex-wrap:wrap;gap:6px'><select id='cidCat'><option>L</option><option>D</option><option>Lab</option><option>Amfi</option><option>Amfi Derslik</option></select><input id='cidName' placeholder='Ad' maxlength='24'><input id='cidHex' placeholder='HEX' maxlength='8' style='width:120px'><button type='button' id='cidAddBtn'>Ekle</button></div><div id='cidAddMsg' style='font-size:12px;opacity:.75;margin-top:4px'></div></details>";
  companyFieldset.appendChild(addBox);
  document.getElementById('cidAddBtn').addEventListener('click',()=>{
    const cat=document.getElementById('cidCat').value.trim();
    const name=document.getElementById('cidName').value.trim();
    const hex=document.getElementById('cidHex').value.trim().toUpperCase();
    const msg=document.getElementById('cidAddMsg');
    if(!/^([0-9A-F]{4,})$/.test(hex)){ msg.textContent='Geçersiz HEX'; msg.style.color='#f88'; return; }
    if(!name){ msg.textContent='Ad gerekli'; msg.style.color='#f88'; return; }
    msg.textContent='Gönderiliyor...'; msg.style.color='#888';
    fetch('/cid_add',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'cat='+encodeURIComponent(cat)+'&name='+encodeURIComponent(name)+'&id='+hex})
      .then(r=>r.json()).then(o=>{ if(o.status==='OK'){ msg.textContent='Eklendi. Yenileniyor...'; msg.style.color='#8df59f'; setTimeout(()=>location.reload(),600); } else { msg.textContent='Hata: '+o.status; msg.style.color='#f88'; } })
      .catch(()=>{ msg.textContent='Bağlantı hatası'; msg.style.color='#f88'; });
  });
})();

// Delete selected company id (removes from single company file)
(function(){
  const sel=document.getElementById('companySelect'); if(!sel) return;
  const parent=sel.parentElement; const delBtn=document.createElement('button');
  delBtn.type='button'; delBtn.textContent='Sil'; delBtn.style.marginLeft='6px'; parent.appendChild(delBtn);
  delBtn.addEventListener('click',()=>{
    const o = sel.options[sel.selectedIndex]; if(!o) { alert('Öğe seçin'); return; }
    if(!confirm('Seçili company silinsin mi?')) return;
    const id = o.value;
    fetch('/cid_del',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'id='+encodeURIComponent(id)})
      .then(r=>r.json()).then(o=>{ if(o.status==='OK') location.reload(); else alert('Silinemedi: '+o.status); })
      .catch(()=>alert('Ağ hatası'));
  });
})();

// Schedule editor
(function(){
  const edit=document.getElementById('schedEdit'); const btn=document.getElementById('saveSched'); const msg=document.getElementById('schedMsg');
  if(edit){ fetch('/schedule_data').then(r=>r.text()).then(t=>{ edit.value=t.trim(); }); }
  if(btn){ btn.addEventListener('click',()=>{
    btn.disabled=true; msg.textContent='Kaydediliyor...';
    fetch('/schedule_save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'data='+encodeURIComponent(edit.value)})
      .then(r=>r.text()).then(t=>{ if(t==='OK'){ msg.textContent='Kaydedildi'; msg.style.color='#8df59f'; setTimeout(()=>location.reload(),600); } else { msg.textContent='Hata: '+t; msg.style.color='#f88'; } btn.disabled=false; })
      .catch(()=>{ msg.textContent='Bağlantı hatası'; msg.style.color='#f88'; btn.disabled=false; });
  }); }
})();

// Active schedule row highlight and sync select
(function(){
  const meta=document.querySelector('meta[name="active-label"]');
  const activeLabel=meta?meta.getAttribute('content'):null;
  if(activeLabel){
    document.querySelectorAll('.schedRow').forEach(r=>{
      if(r.getAttribute('data-label') && r.getAttribute('data-label').toLowerCase()===activeLabel.toLowerCase()){
        r.classList.add('activeRow');
      }
    });
  }
  const sel=document.getElementById('companySelect'); const inp=document.getElementById('companyInput');
  if(sel&&inp){
    const cur=inp.value.trim().toUpperCase();
    for(let i=0;i<sel.options.length;i++){ if(sel.options[i].value.toUpperCase()===cur){ sel.selectedIndex=i; break; } }
    // Keep the select and input in sync: when user picks from the list, update the input
    sel.addEventListener('change',()=>{
      const v = (sel.options[sel.selectedIndex] && sel.options[sel.selectedIndex].value) || '';
      inp.value = v;
      // dispatch input so any sanitizer/listeners react
      inp.dispatchEvent(new Event('input', { bubbles: true }));
    });
  }
})();

// Schedule force select handler
(function(){
  const resBox=document.getElementById('result');
  document.querySelectorAll('.schedRow').forEach(r=>r.addEventListener('click',()=>{
    const lab=r.getAttribute('data-label');
    fetch('/schedule_force',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'label='+encodeURIComponent(lab)})
      .then(res=>res.json()).then(j=>{
        resBox.style.display='block';
        if(j.status==='OK'){
          resBox.style.background='#1d4027'; resBox.style.borderColor='#255c33'; resBox.style.color='#8df59f';
          resBox.textContent='Aktif: '+lab;
          const inp=document.getElementById('companyInput'); const sel=document.getElementById('companySelect');
          if(inp){ inp.value=j.id; inp.dispatchEvent(new Event('input')); }
          if(sel){ for(let i=0;i<sel.options.length;i++){ if(sel.options[i].value.toUpperCase()===j.id.toUpperCase()){ sel.selectedIndex=i; break; } } }
          const modeLabel=document.getElementById('modeVal'); if(modeLabel && j.mode){ modeLabel.textContent=j.mode; }
        } else {
          resBox.style.background='#442226'; resBox.style.borderColor='#6a2a30'; resBox.style.color='#ffb2b9';
          resBox.textContent='Hata: '+(j.status||'Bilinmeyen');
        }
      })
      .catch(()=>{});
  }));
})();
