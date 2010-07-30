import unittest
from jstreeExample.jstree.models import Cidade, Estado

class CidadeTestCase(unittest.TestCase):
    def setUp(self):
        self.poa = Cidade.objects.create(nome="Porto Alegre")
        self.rs = Estado.objects.create(nomeEstado="Rio Grande do Sul",uf="RS",capital=self.poa)
        self.poa.estado = self.rs
        self.poa.save()
        

    def testRelation(self):
        self.assertEquals(self.rs.capital, self.poa)
        self.assertEquals(self.poa.estado, self.rs)

