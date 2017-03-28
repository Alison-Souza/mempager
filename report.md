# PAGINADOR DE MEMÓRIA - RELATÓRIO

1. Termo de compromisso

Os membros do grupo afirmam que todo o código desenvolvido para este
trabalho é de autoria própria.  Exceto pelo material listado no item
3 deste relatório, os membros do grupo afirmam não ter copiado
material da Internet nem ter obtido código de terceiros.

2. Membros do grupo e alocação de esforço

Preencha as linhas abaixo com o nome e o e-mail dos integrantes do
grupo.  Substitua marcadores `XX` pela contribuição de cada membro
do grupo no desenvolvimento do trabalho (os valores devem somar
100%).

  * Alison de Oliveira Souza <alison.souza@dcc.ufmg.br> 50%
  * Daniel Reis Souza <daniel.reis@dcc.ufmg.br> 50%

3. Referências bibliográficas

    1 - Informações sobre sysconf() - http://man7.org/linux/man-pages/man3/sysconf.3.html

4. Estruturas de dados

  1. Descreva e justifique as estruturas de dados utilizadas em sua
     solução.
	
	Utilizamos um vetor chamado table_list do tipo table_list_t para 
    criar a lista de tabelas de páginas de todos os processos. Cada
    posição desse vetor comtém um pid, que identifica o processo e um
    ponteiro para a tabela de páginas desse processo.

    Também utilizamos um vetor chamado block_vector do tipo int que 
    serve como um indicador dos blocos do disco. Como não era necessário
    controlar o disco, esse vetor serve apenas para indicar se o blocos 
    está em uso ou não.

    E ainda temos o vetor frame_vector do tipo frame_t, que implementa a
    lista de quadros da memória. Cada quadro (posição do vetor) contém
    o pid do processo que aloca este quadro, um page_number que indica
    o número da última página alocada, e as flags free_frame, que indica
    se o quadro está ou não em uso, reference_bit, usado no algoritmo 
    da segunda chance, none e wrote
    ************* ESCREVA SOBRE AS FLAGS NONE E WROTE ******************

  2. Descreva o mecanismo utilizado para controle de acesso
     e modificação às páginas.
    
    O controle de acesso e a modificação das páginas ocorre na função 
    pager_fault(). Quando uma página é alocada pela primeira vez ela
    recebe permissão de leitura apenas. Caso o processo queira escrever
    nesta página, é gerado um page fault, e a página recebe permissão de
    escrita. 
    Quando a memória fica cheia... e o none...
********* FALE SOBRE O NONE NO PAGER_FAULT() ***************




