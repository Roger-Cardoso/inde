"""Exportação de leitura linear: somente capítulos públicos, sem conexão ou scripts."""
from html import escape
from html.parser import HTMLParser
from pathlib import Path
import base64
import mimetypes

class Fragmento(HTMLParser):
    def __init__(self, base, root, prefixo=""):
        self.prefixo=prefixo
        super().__init__(convert_charrefs=True);self.base=base;self.root=root;self.saida=[];self.ignorar=0
    def handle_starttag(self,tag,attrs):
        if tag in ('script','style','iframe','object'):self.ignorar+=1;return
        if self.ignorar or tag in ('html','head','body','meta','link','embed'):return
        novos=[]
        for k,v in attrs:
            if k.startswith('on') or k in ('srcdoc','style','srcset','poster','background','action','formaction','ping'):continue
            if k=='id' and v:v=self.prefixo+v
            if k=='href' and v and v.startswith('#'):v='#'+self.prefixo+v[1:]
            if k=='src':
                if tag!='img':continue
                if not v:continue
                if v.startswith('data:image/'):pass
                elif '://' in v or v.startswith('//'):continue
                else:
                    p=(self.root/v.lstrip('/') if v.startswith('/') else self.base/v).resolve()
                    if not p.is_relative_to(self.root.resolve()) or not p.is_file():continue
                    mime=mimetypes.guess_type(p.name)[0] or ''
                    if not mime.startswith('image/'):continue
                    v=f'data:{mime};base64,'+base64.b64encode(p.read_bytes()).decode()
            if k=='href' and (not v or not v.startswith('#')):continue
            novos.append((k,v))
        self.saida.append('<'+tag+''.join(' '+k+(('="'+escape(v,quote=True)+'"') if v is not None else '') for k,v in novos)+'>')
    def handle_startendtag(self,tag,attrs):self.handle_starttag(tag,attrs)
    def handle_endtag(self,tag):
        if tag in ('script','style','iframe','object'):
            self.ignorar=max(0,self.ignorar-1);return
        if not self.ignorar and tag not in ('html','head','body','meta','link','embed'):self.saida.append('</'+tag+'>')
    def handle_data(self,data):
        if not self.ignorar:self.saida.append(escape(data))

def exportar(info,estrutura,arquivos,root):
    caps=[c for s in estrutura['sagas'] for v in s['volumes'] for a in v['arcos'] for c in a['capitulos']]
    fontes=dict(arquivos);partes=[]
    for c in caps:
        p=fontes[c['arquivo']];parser=Fragmento(p.parent,root,c['id']+'--');parser.feed(p.read_text(encoding='utf-8'))
        partes.append(f'<section id="{escape(c["id"],quote=True)}"><h2>{escape(c["titulo"])}</h2>'+''.join(parser.saida)+'</section>')
    indice=''.join(f'<li><a href="#{escape(c["id"],quote=True)}">{escape(c["titulo"])}</a></li>' for c in caps)
    return '<!doctype html><html lang="pt-BR"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>'+escape(info['titulo'])+' — Edição offline</title><style>body{max-width:720px;margin:50px auto;padding:0 24px;background:#f6f3eb;color:#29312b;font:19px/1.85 Georgia,serif}h1,h2{line-height:1.2;font-weight:normal}section{margin:70px 0}a{color:#256b58}p{orphans:3;widows:3}img{max-width:100%}details{padding:16px;border:1px solid #ccc;margin:25px 0}small{font:12px/1.7 sans-serif}@media print{body{background:white;color:black;font-size:12pt}nav{display:none}section{break-before:page}h2{break-after:avoid}}</style><header><small>EDIÇÃO · LEITURA OFFLINE</small><h1>'+escape(info['titulo'])+'</h1><p>'+escape(info['autor'])+'</p><small>Somente capítulos publicados. Notas, glossário interativo, preferências e progresso do site não acompanham esta edição. Imagens externas não são baixadas.</small></header><nav aria-label="Sumário"><ol>'+indice+'</ol></nav><main>'+''.join(partes)+'</main></html>'
