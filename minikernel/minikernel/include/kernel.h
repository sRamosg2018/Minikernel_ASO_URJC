/*
 *  minikernel/include/kernel.h
 *
 *  Minikernel. Versión 1.0
 *
 *  Fernando Pérez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene definiciones usadas por kernel.c
 *
 *      SE DEBE MODIFICAR PARA INCLUIR NUEVA FUNCIONALIDAD
 *
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#include "const.h"
#include "HAL.h"
#include "llamsis.h"

/*
 *
 * Definicion del tipo que corresponde con el BCP.
 * Se va a modificar al incluir la funcionalidad pedida.
 *
 */


#define NO_RECURSIVO 0
#define RECURSIVO 1

#define OCUPADO 0 
#define LIBRE 1


typedef struct{

        char* nombre[MAX_NOM_MUT]; 
        int estado;  //LISTO OCUPADO
        int tipo;//RECURSIVO NO RECURSIVO
        int id_propietario;      
        int num_veces_lock;

        lista_BCPs procesos_espera; //Procesos bloqueados porque no tenian el Mutex  
}mutex;

int total_mutex;
mutex lista_mutex[NUM_MUT];







typedef struct BCP_t *BCPptr;           

typedef struct BCP_t {
        int id;				/* ident. del proceso */
        int estado;			/* TERMINADO|LISTO|EJECUCION|BLOQUEADO*/
        contexto_t contexto_regs;	/* copia de regs. de UCP */
        void * pila;			/* dir. inicial de la pila */
	BCPptr siguiente;		/* puntero a otro BCP */
	void *info_mem;			/* descriptor del mapa de memoria */
        //nuevos
        unsigned int segundos;          //SLEEP

        int ticks_rodaja;               //ROUNDROBIN

        mutex* descriptores[NUM_MUT_PROC]; //MUTEX
        int num_descriptores;          //MUTEX       

} BCP;

/*
 *
 * Definicion del tipo que corresponde con la cabecera de una lista
 * de BCPs. Este tipo se puede usar para diversas listas (procesos listos,
 * procesos bloqueados en semáforo, etc.).
 *
 */

typedef struct{
	BCP *primero;
	BCP *ultimo;
} lista_BCPs;


/*
 * Variable global que identifica el proceso actual
 */

BCP * p_proc_actual=NULL;

/*
 * Variable global que representa la tabla de procesos
 */

BCP tabla_procs[MAX_PROC];

/*
 * Variable global que representa la cola de procesos listos
 */
lista_BCPs lista_listos= {NULL, NULL};
lista_BCPs lista_bloqueados= {NULL, NULL};
lista_BCPs lista_bloqueados_mutex= {NULL, NULL};


/*
 *
 * Definición del tipo que corresponde con una entrada en la tabla de
 * llamadas al sistema.
 *
 */
typedef struct{
	int (*fservicio)();
} servicio;


/*
 * Prototipos de las rutinas que realizan cada llamada al sistema
 */
int sis_crear_proceso();
int sis_terminar_proceso();
int sis_escribir();
int obtener_id_pr();
int dormir(unsigned int segundos);

int crear_mutex(char *nombre, int tipo);
int abrir_mutex(char *nombre);
int lock(unsigned int mutexid);
int unlock(unsigned int mutexid);
int cerrar_mutex(unsigned int mutexid);

/*
 * Variable global que contiene las rutinas que realizan cada llamadach
 */
servicio tabla_servicios[NSERVICIOS]={	{sis_crear_proceso},
					{sis_terminar_proceso},
					{sis_escribir},
					{obtener_id_pr},
                                        {dormir},
                                        {crear_mutex},
                                        {abrir_mutex},
                                        {lock},
                                        {unlock},
                                        {cerrar_mutex}                         
                                     };

#endif /* _KERNEL_H */

