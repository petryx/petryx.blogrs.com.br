from django.db import models

# Create your models here.

class Estado(models.Model):
    nomeEstado = models.CharField(max_length=50)
    uf= models.CharField(max_length=2)

class Cidade(models.Model):
    nome = models.CharField(max_length=50)
    estado = models.ForeignKey("Estado",related_name='cidades')