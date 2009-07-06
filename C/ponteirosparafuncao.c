#include <stdio.h>
#include <stdlib.h>

float adicao(float a, float b) {return a+b;};
float subtracao(float a, float b) {return a-b;};
float multiplicacao(float a, float b) {return a*b;};
float divisao(float a, float b) {return a/b;};

int main()
{
   float(*pt2Func[4])(float, float) = {NULL};
   float(*Func)(float, float) = NULL;

   pt2Func['+'] = &adicao;
   pt2Func['-'] = &subtracao;
   pt2Func['/'] = &divisao;
   pt2Func['*'] = &multiplicacao;

   float a;
   float b;
   char operator;

    while(1)
    {
           printf("Informe a: ");
           scanf("%f",&a);

           printf("\nInforme operador: ");
           scanf(" %c",&operator);

           printf("\nInforme b: ");
           scanf(" %f",&b);

           if(operator != '-' && operator != '+' && operator != '*' && operator != '/' )
           {
                    printf("Operação não implementada");
                    exit(-1);
           }

           Func = pt2Func[operator];
           printf("\nResultado: %f\n", Func(a,b));
           printf("\nDeseja Continuar (y/n):");
           scanf(" %c",&operator);

           if(operator == 'n' || operator == 'N')
             break;
           Func = NULL;

    }
    exit(0);
}