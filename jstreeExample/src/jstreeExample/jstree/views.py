# Create your views here.
from django.shortcuts import get_object_or_404, render_to_response
from django.http import HttpResponseRedirect, HttpResponse
from django.core.urlresolvers import reverse
from django.template import RequestContext
from jstreeExample.jstree.models import *
import datetime
from django.utils import simplejson

def main(request):
    current_date = datetime.datetime.now()
    return render_to_response('base.html', locals())

def jstreeHTML(request):
    estados = Estado.objects.all()
    response = HttpResponse()
    html = "<ul>"
    html += "<li><a href=#>Estados</a></li>"
    
    for e in estados:
        html += "<li><a href=#>%s</a></li>" % str(e.nomeEstado)
        cidades = e.cidades.all()
        for c in cidades:
            html += "<li>%s</li>" % str(c.nome)
    html += "</ul>"
    response.write(html)
    return response

def jstreeJson(request):
    estados = Estado.objects.all()
    treeJson = []
    for e in estados:
        cidades = e.cidades.all()
        cids = []
        for c in cidades:
            cids.append( {'data' : str(c.nome) , "attr" :{  "id" :  str(c.nome) }})
        treeJson.append({'data' : str(e.nomeEstado), 'children' : cids,
                          })
    json = simplejson.dumps(treeJson)
    return HttpResponse(json,mimetype="application/json")         