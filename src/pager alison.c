#include "pager.h"

#include <sys/mman.h>

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mmu.h"

//**************************************************************//
//***               Estruturas auxiliares                    ***//
//**************************************************************//

// Estrutura dos quadros da memória.
typedef struct frame
{
    pid_t pid;
    int page_number;
    short free_frame; // 0 se não está em uso, 1 caso contrário.
    short reference_bit; // Usado no algoritmo de segunda chance (0 - substitui, 1 - 2ª chance)
} frame_t;

// Estrutura das tabelas de páginas de cada processo.
typedef struct page_table
{
    int num_pages; // Usado como tamanho dos dois vetores frames e blocks.
    int *frames; // Vetor de quadros do processo.
    int *blocks; // Vetor de blocos do processo.
} page_table_t;

// Estrutura da lista de tabela de páginas.
typedef struct table_list
{
    pid_t pid;
    page_table_t *table;
} table_list_t;

// Vetor dos blocos do disco.
int *block_vector;
int free_blocks; // informa quantos blocos ainda estão livres.
int size_block_vector;

// Vetor dos quadros da memória.
frame_t *frame_vector;
int clock_ptr; // Usado no algoritmo da segunda chance.
int size_frame_vector;

// Vetor de tabela de páginas.
table_list_t *table_list;
int size_table_list; // Mantem o tamanho do vetor da tabela de páginas.

//**************************************************************//
//***                   Funções auxiliares                   ***//
//**************************************************************//



/***************************************************************
 * external functions
 **************************************************************/

/* Inicializa as estruturas de dados necessárias.
 * nframes = números de quadros da memória
 * nblocks = números de blocos do disco */
void pager_init(int nframes, int nblocks)
{
    int i;
    size_table_list = 1;

    // Inicialização dos vetores.
    block_vector = (int*) malloc (nblocks * sizeof(int));
    frame_vector = (frame_t*) malloc (nframes * sizeof(frame_t));
    table_list = (table_list_t*) malloc (size_table_list * sizeof(table_list_t));

    //Número de blocos livres iniciais = todos os blocos.
    free_blocks = nblocks;
    size_block_vector = nblocks;
    for(i = 0; i < nblocks; i++)
        block_vector[i] = 0; //Setando todas os blocos como vazios.

    // Ponteiro do relógio usado no algoritmo da segunda chance.
    clock_ptr = 0;
    size_frame_vector = nframes;
    for(i = 0; i < nframes; i++)
    {
        frame_vector[i].pid = -1;
        frame_vector[i].page_number = 0;
        frame_vector[i].free_frame = 0; //Setando todos os frames como vazios.
        frame_vector[i].reference_bit = 0;
    }
}

/* Inicializa as Estruturas de Dados para alocar e gerenciar
 * memória a um novo processo */
void pager_create(pid_t pid)
{
    int i, j, num_pages, flag = 0;
    // Calcula o número de páginas dos vetores frames e blocks.
    num_pages = (UVM_MAXADDR - UVM_BASEADDR + 1) / sysconf(_SC_PAGESIZE);

    // Procurando tabela vazia na lista de tabelas.
    for(i = 0; i < size_table_list; i++){
        // Se a tabela estiver vazia.
        if(table_list[i].table == NULL)
        {
            // Atribui o pid do processo a tabela.
            table_list[i].pid = pid;
            // Inicializa a tabela de página.
            table_list[i].table = (page_table_t*) malloc (sizeof(page_table_t));
            table_list[i].table->num_pages = num_pages;
            // Inicializa os vetores frames e blocks.
            table_list[i].table->frames = (int*) malloc (num_pages * sizeof(int));
            table_list[i].table->blocks = (int*) malloc (num_pages * sizeof(int));

            // Seta os valores para -1 (convenção de vazio).
            for(j = 0; j < num_pages; j++)
            {
                table_list[i].table->frames[j] = -1;
                table_list[i].table->blocks[j] = -1;
            }
            flag = 1; // Flag usada para indicar se achou uma tabela vazia.
            break;
        }
    }

    // Se não achou uma tabela vazia.
    if(flag == 0)
    {
        // Aumenta o tamanho da lista de tabelas.
        table_list = realloc(table_list, (100 + size_table_list) * sizeof(table_list_t));

        // Aloca o novo processo a primeira posição vazia.
        table_list[size_table_list].pid = pid;
        table_list[size_table_list].table = (page_table_t*) malloc (sizeof(page_table_t));
        table_list[size_table_list].table->num_pages = num_pages;
        table_list[size_table_list].table->frames = (int*) malloc (num_pages * sizeof(int));
        table_list[size_table_list].table->blocks = (int*) malloc (num_pages * sizeof(int));
        for(j = 0; j < num_pages; j++)
        {
            table_list[size_table_list].table->frames[j] = -1;
            table_list[size_table_list].table->blocks[j] = -1;
        }
        j=size_table_list + 1;
        size_table_list += 100;
        for(; j < size_table_list; j++)
        {
            table_list[j].table = NULL;
        }
    }
}
/* Reserva um bloco do disco para a nova página e retorna o
 * endereço virtual da página de memória, caso não haja mais
 * blocos livres a função deve retornar NULL.  */
void *pager_extend(pid_t pid)
{
    // Se não houver blocos livres no disco, retorne NULL.
    if(free_blocks == 0)
        return NULL;

    int i, j, block;

    // Pegando o primeiro bloco vazio.
    for(i = 0; i < size_block_vector; i++)
    {
        if(block_vector[i] == 0) //Se o bloco na posição i não é usado.
        {
            block_vector[i] = 1; // Seta o bloco para usado.
            free_blocks--; // Decrementa o contador de blocos livres.
            block = i; // Variável auxiliar para salvar a posição do bloco.
            break;
        }
    }

    // Localizando a tabela de página do processo pid.
    for(i = 0; i < size_table_list; i++)
    {
        if(table_list[i].pid == pid)
        {
            // Procuro um bloco vazio na tabela do processo.
            for(j = 0; j < table_list[i].table->num_pages; j++)
            {
                if(table_list[i].table->blocks[j] == -1)
                {
                    // Salvo o indice do vetor de blocos, no bloco do processo.
                    table_list[i].table->blocks[j] = block;
                    break;
                }

                // Se não há blocos livres na tabela do processo (É preciso isso????)
                if(j == (table_list[i].table->num_pages) - 1)
                    return NULL;
            }
            break;
        }
    }

    // Retorna o endereço (Inicio + posição * tamanho da pagina).
    return (void*) (UVM_BASEADDR + (intptr_t) (j * sysconf(_SC_PAGESIZE)));
}

/* Trata falhas de acesso a memória.
 * Recebe o id do processo e o endereço virtual e usa funções do mmu
 * para recuperar um bloco livre e permitir o acesso ao vaddr pelo pid */
void pager_fault(pid_t pid, void *vaddr)
{
    int i, index, index2, page_num, curr_frame, new_frame, curr_block, new_block,
        move_disk_pid, move_disk_pnum, mem_full;
    void *addr;

    // Procura o índice da tabela de página do processo pid na lista de tabelas.
    for(i = 0; i < size_table_list; i++)
    {
        if(table_list[i].pid == pid)
        {
            // Salva o indice e sai.
            index = i;
            break;
        }
    }

    // Pega o número do quadro na tabela do processo.
    page_num = ((((intptr_t) vaddr) - UVM_BASEADDR) / (sysconf(_SC_PAGESIZE)));

    mem_full = 1;
    for(i = 0; i < size_frame_vector; i++)
    {
        if(frame_vector[i].free_frame == 0)
        {
            mem_full = 0;
            break;
        }
    }
    
    if(mem_full)
    {
        for(i = 0; i < size_frame_vector; i++)
        {
            addr = (void*) (UVM_BASEADDR + (intptr_t) (frame_vector[i].page_number * sysconf(_SC_PAGESIZE)));
            mmu_chprot(frame_vector[i].pid, addr, PROT_NONE);
        }
        //Fazer um for sobre table-list[index]table->frames[i] e setar para -1
    }

    // Atenção nesse if: as primeiras execuções NUNCA entram aqui (e nem devem)...
    // Esse if é usado em caso de page fault de permissão, ou seja, se 
    // você quer escrever em uma página que não tem permissão de escrita.
    // Se esse quadro está em uso.
    if(table_list[index].table->frames[page_num] != -1)
    {
        // Salva o índice do vetor de quadros (memória).
        curr_frame = table_list[index].table->frames[page_num];
        // Dá permissão de escrita para o processo pid.
        mmu_chprot(pid, vaddr, PROT_READ | PROT_WRITE);
        // Marca o bit de referência no vetor de quadros (memória).
        frame_vector[curr_frame].reference_bit = 1;
    }
    else // Se não está em uso:
    {
        new_frame = -1;
        while(new_frame == -1)
        {
            new_frame = -1;
            // Se o bit de referência é zero.
            if(frame_vector[clock_ptr].reference_bit == 0)
            {
                new_frame = clock_ptr;
                // Se o frame está em uso.
                if(frame_vector[clock_ptr].free_frame == 1)
                {
                    // Salva o frame no disco.
                    move_disk_pid = frame_vector[clock_ptr].pid;
                    move_disk_pnum = frame_vector[clock_ptr].page_number;
                    for(i = 0; i < size_table_list; i++)
                    {
                        if(table_list[i].pid == move_disk_pid)
                        {
                            index2 = i;
                        }
                    }
                    curr_block = table_list[index2].table->blocks[move_disk_pnum];
                    //mmu_disk_write(clock_ptr, curr_block);
                    mmu_nonresident(pid, (void*) (UVM_BASEADDR + (intptr_t) (move_disk_pnum * sysconf(_SC_PAGESIZE))));
                    // Marca o frame como vazio (sem uso).
                    table_list[index2].table->frames[page_num] = -1;
                }
                // Coloca o novo processo no vetor de quadros.
                frame_vector[clock_ptr].pid = pid;
                frame_vector[clock_ptr].page_number = page_num;
                frame_vector[clock_ptr].free_frame = 1;
                frame_vector[clock_ptr].reference_bit = 1;
            }
            else
            {
                frame_vector[clock_ptr].reference_bit = 0;
            }
            clock_ptr++;
            clock_ptr %= size_frame_vector;
        }
        // Esse if está certo???
        if(table_list[index].table->blocks[page_num] == -1)
        {
            new_block = table_list[index].table->blocks[page_num];
            mmu_disk_read(new_block, new_frame);
        }
        table_list[index].table->frames[page_num] = new_frame;
        mmu_zero_fill(new_frame);
        mmu_resident(pid, vaddr, new_frame, PROT_READ /*| PROT_WRITE*/);
        
        /*if(table_list[index].table->blocks[page_num] != -1)
        {
            new_block = table_list[index].table->blocks[page_num];
            mmu_zero_fill(new_frame);
            table_list[index].table->frames[page_num] = new_frame;
            mmu_resident(pid, vaddr, new_frame, PROT_READ);
        }*/
    }
}


/* Copia len bytes a partir de addr para uma string e imprime. */
int pager_syslog(pid_t pid, void *addr, size_t len)
{
 /*   int i, j, index, frame_limit, num_frames;
    char *message = (char*) malloc (len + 1);

    for(i = 0; i < size_table_list; i++)
    {
        if(table_list[i].pid == pid)
        {
            index = i;
            break;
        }
    }

    num_frames = (UVM_MAXADDR - UVM_BASEADDR + 1) / sysconf(_SC_PAGESIZE);

    for(i = 0; i < num_frames; i++)
    {
        if(table_list[index].table->frames[i] == 0)
        {
            frame_limit = i;
            break;
        }
    }



    for(i = 0; i < len; i++)
    {
        for(j = 0; j < frame_limit; j++)
        {

        }
        // Se o processo não tem permissão de acessar a posição addr+i
        if()
    }
*/
    //printf("pager_syslog pid %d %s\n", (int)pid, message);
    return 0; // Caso sucesso
    return -1; // Caso a string message[len] passe o espaço alocado
               // por pager_extend
}

/* É chamada quando um programa termina de executar. Não precisa
 * chamar funções do mmu.h, basta atualizar as informações do
 * paginador para que seja possível reutilizar os quadros alocados. */
void pager_destroy(pid_t pid)
{
    int i;
    for(i =0; i < size_table_list; i++)
    {
        if(table_list[i].pid == pid)
        {
            table_list[i].pid = 0;
            free(table_list[i].table->frames);
            free(table_list[i].table->blocks);
            free(table_list[i].table);
            table_list[i].table = NULL;
            free_blocks++;
        }
    }
}

/*  */
void pager_free(void)
{
}

