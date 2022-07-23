/*
 *  kernel/kernel.c
 *
 *  Minikernel. Versión 1.0
 *
 *  Fernando Pérez Costoya
 *
 */

/*
 *
 * Fichero que contiene la funcionalidad del sistema operativo
 *
 */

#include "kernel.h"	/* Contiene defs. usadas por este modulo */

/*
 *
 * Funciones relacionadas con la tabla de procesos:
 *	iniciar_tabla_proc buscar_BCP_libre
 *
 */



/*
 * Función que inicia la tabla de procesos
 */
static void iniciar_tabla_proc(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		tabla_procs[i].estado=NO_USADA;
}

/*
 * Función que busca una entrada libre en la tabla de procesos
 */
static int buscar_BCP_libre(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		if (tabla_procs[i].estado==NO_USADA)
			return i;
	return -1;
}

/*
 *
 * Funciones que facilitan el manejo de las listas de BCPs
 *	insertar_ultimo eliminar_primero eliminar_elem
 *
 * NOTA: PRIMERO SE DEBE LLAMAR A eliminar Y LUEGO A insertar
 */

/*
 * Inserta un BCP al final de la lista.
 */
static void insertar_ultimo(lista_BCPs *lista, BCP * proc){
	if (lista->primero==NULL)
		lista->primero= proc;
	else
		lista->ultimo->siguiente=proc;
	lista->ultimo= proc;
	proc->siguiente=NULL;
}

/*
 * Elimina el primer BCP de la lista.
 */
static void eliminar_primero(lista_BCPs *lista){

	if (lista->ultimo==lista->primero)
		lista->ultimo=NULL;
	lista->primero=lista->primero->siguiente;
}

/*
 * Elimina un determinado BCP de la lista.
 */
static void eliminar_elem(lista_BCPs *lista, BCP * proc){
	BCP *paux=lista->primero;

	if (paux==proc)
		eliminar_primero(lista);
	else {
		for ( ; ((paux) && (paux->siguiente!=proc));
			paux=paux->siguiente);
		if (paux) {
			if (lista->ultimo==paux->siguiente)
				lista->ultimo=paux;
			paux->siguiente=paux->siguiente->siguiente;
		}
	}
}

/*
 *
 * Funciones relacionadas con la planificacion
 *	espera_int planificador
 */

/*
 * Espera a que se produzca una interrupcion
 */
static void espera_int(){
	int nivel;

//	printk("-> NO HAY LISTOS. ESPERA INT\n");

	/* Baja al mínimo el nivel de interrupción mientras espera */
	nivel=fijar_nivel_int(NIVEL_1);
	halt();
	fijar_nivel_int(nivel);
}

/*
 * Función de planificacion que implementa un algoritmo FIFO.
 */
static BCP * planificador(){
	while (lista_listos.primero==NULL)
		espera_int();
	lista_listos.primero->ticks_rodaja=TICKS_POR_RODAJA;
	return lista_listos.primero;
}

/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso(){
	BCP * p_proc_anterior;

	liberar_imagen(p_proc_actual->info_mem); /* liberar mapa */

	p_proc_actual->estado=TERMINADO;
	eliminar_primero(&lista_listos); /* proc. fuera de listos */

	/* Realizar cambio de contexto */
	p_proc_anterior=p_proc_actual;
	p_proc_actual=planificador();

	printk("-> C.CONTEXTO POR FIN: de %d a %d\n",
			p_proc_anterior->id, p_proc_actual->id);

	liberar_pila(p_proc_anterior->pila);
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
        return; /* no debería llegar aqui */
}

/*
 *
 * Funciones relacionadas con el tratamiento de interrupciones
 *	excepciones: exc_arit exc_mem
 *	interrupciones de reloj: int_reloj
 *	interrupciones del terminal: int_terminal
 *	llamadas al sistemas: llam_sis
 *	interrupciones SW: int_sw
 *
 */

/*
 * Tratamiento de excepciones aritmeticas
 */
static void exc_arit(){

	if (!viene_de_modo_usuario())
		panico("excepcion aritmetica cuando estaba dentro del kernel");


	printk("-> EXCEPCION ARITMETICA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no debería llegar aqui */
}

/*
 * Tratamiento de excepciones en el acceso a memoria
 */
static void exc_mem(){

	if (!viene_de_modo_usuario())
		panico("excepcion de memoria cuando estaba dentro del kernel");


	printk("-> EXCEPCION DE MEMORIA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no debería llegar aqui */
}

/*
 * Tratamiento de interrupciones de terminal
 */
static void int_terminal(){
	//char car;
	//car = leer_puerto(DIR_TERMINAL);
	//printk("->TRATANDO INT. DE TERMINAL %c\n", car);

        return;
}

/*
 * Tratamiento de interrupciones de reloj
 */

static void int_reloj(){
//printk("-> TRATANDO INT. DE RELOJ\n");

//PROCESOS ROUND ROBIN

p_proc_actual->ticks_rodaja--;
if(p_proc_actual->ticks_rodaja<=0){
	activar_int_SW();
}

//PROCESOS DORMIDOS

	BCP *p_proc_dormir = lista_bloqueados.primero;
	while(p_proc_dormir != NULL){ 
		p_proc_dormir->segundos--;
		if(p_proc_dormir->segundos <= 0){ 


			int nivel=fijar_nivel_int(NIVEL_3);

			p_proc_dormir->estado = LISTO;
			p_proc_dormir->segundos = 0;

			eliminar_elem(&lista_bloqueados, p_proc_dormir);
			insertar_ultimo(&lista_listos, p_proc_dormir);
			fijar_nivel_int(nivel);
		}
		p_proc_dormir = p_proc_dormir->siguiente;
	}

    return;
}

/*
 * Tratamiento de llamadas al sistema
 */
static void tratar_llamsis(){
	int nserv, res;

	nserv=leer_registro(0);
	if (nserv<NSERVICIOS)
		res=(tabla_servicios[nserv].fservicio)();
	else
		res=-1;		/* servicio no existente */
	escribir_registro(0,res);
	return;
}

/*
 * Tratamiento de interrupciuones software
 */
static void int_sw(){

	printk("-> TRATANDO INT. SW\n");
	//int nivel = fijar_nivel_int(NIVEL_1);
	int nivel = fijar_nivel_int(NIVEL_3);
	BCP* proceso_rr=p_proc_actual;
	eliminar_primero(&lista_listos);
	insertar_ultimo(&lista_listos, proceso_rr);
	p_proc_actual=planificador();
	
	cambio_contexto(&(proceso_rr->contexto_regs), &(p_proc_actual->contexto_regs));
	fijar_nivel_int(nivel);


	return;
}

/*
 *
 * Funcion auxiliar que crea un proceso reservando sus recursos.
 * Usada por llamada crear_proceso.
 *
 */
static int crear_tarea(char *prog){
	void * imagen, *pc_inicial;
	int error=0;
	int proc;
	BCP *p_proc;

	proc=buscar_BCP_libre();
	if (proc==-1)
		return -1;	/* no hay entrada libre */

	/* A rellenar el BCP ... */
	p_proc=&(tabla_procs[proc]);

	/* crea la imagen de memoria leyendo ejecutable */
	imagen=crear_imagen(prog, &pc_inicial);
	if (imagen)
	{
		p_proc->info_mem=imagen;
		p_proc->pila=crear_pila(TAM_PILA);
		fijar_contexto_ini(p_proc->info_mem, p_proc->pila, TAM_PILA,
			pc_inicial,
			&(p_proc->contexto_regs));
		p_proc->id=proc;
		p_proc->estado=LISTO;

		/* lo inserta al final de cola de listos */
		insertar_ultimo(&lista_listos, p_proc);
		error= 0;
	}
	else
		error= -1; /* fallo al crear imagen */

	return error;
}

/*
 *
 * Rutinas que llevan a cabo las llamadas al sistema
 *	sis_crear_proceso sis_escribir
 *
 */

/*
 * Tratamiento de llamada al sistema crear_proceso. Llama a la
 * funcion auxiliar crear_tarea sis_terminar_proceso
 */
int sis_crear_proceso(){
	char *prog;
	int res;

	printk("-> PROC %d: CREAR PROCESO\n", p_proc_actual->id);
	prog=(char *)leer_registro(1);
	res=crear_tarea(prog);
	return res;
}

/*
 * Tratamiento de llamada al sistema escribir. Llama simplemente a la
 * funcion de apoyo escribir_ker
 */
int sis_escribir()
{
	char *texto;
	unsigned int longi;

	texto=(char *)leer_registro(1);
	longi=(unsigned int)leer_registro(2);

	escribir_ker(texto, longi);
	return 0;
}

/*
 * Tratamiento de llamada al sistema terminar_proceso. Llama a la
 * funcion auxiliar liberar_proceso
 */
int sis_terminar_proceso(){

	//printk("-> FIN PROCESO %d\n", p_proc_actual->id);

	for(int i = 0; i<p_proc_actual->num_descriptores;i++){
		cerrar_mutex(i);
	}

	liberar_proceso();

        return 0; /* no deber�a llegar aqui */
}

int obtener_id_pr(){
	return p_proc_actual->id;
}

int dormir(unsigned int segundos){
	segundos = (unsigned int)leer_registro(1);
	int nivel;
	nivel=fijar_nivel_int(NIVEL_3);

	p_proc_actual->estado = BLOQUEADO;
	p_proc_actual->segundos =  segundos * TICK; 

	eliminar_elem(&lista_listos, p_proc_actual);
	insertar_ultimo(&lista_bloqueados,p_proc_actual);
	

	BCP *p_proc_dormir = p_proc_actual;
	p_proc_actual = planificador();
	cambio_contexto(&(p_proc_dormir->contexto_regs), &(p_proc_actual->contexto_regs));
	fijar_nivel_int(nivel);
	return 0;
}




//FUNCIONES AUXILIARES MUTEX

int descriptores_libres(){
		for(int i=0;i<NUM_MUT_PROC; i++){
		if(p_proc_actual->array_descriptores[i]==NULL)
			return i; 
	}
	return -1;
}

int mutex_libres(){
			for(int i=0;i<NUM_MUT; i++){
				if(lista_mutex[i]->estado==1){ //1 = LIBRE //if(lista_mutex[i].estado==LIBRE)
					return i;
				}
	return -1;

}



int crear_mutex(char *nombre, int tipo){

	nombre = (char*)leer_registro(1);
	if(strlen(nombre)>MAX_NOM_MUT){printk("Nombre demasiado largo\n");return -1;} //nombre demasiado largo
	tipo = (int)leer_registro(2);

	//En caso de que el nombre sea valido, se debe comprobar si exite ya un mutex con el mismo nombre
	if(total_mutex>0){
		for(int i =0; i<total_mutex;i++){
			if(strcmp(lista_mutex[i]->nombre, nombre == 0)){
				printk("Nombre igual a otro\n");
				return -1;
			}
		}
	}

	int descriptor = descriptores_libres();
	if(descriptor<0){printk("No hay descriptores libres\n");return -1;}

	int mutex = mutex_libres();
	if(mutex<0){
	printk("No hay mutex libres, se procede a bloquear el proceso.\n");

	int nivel = fijar_nivel_int(NIVEL_3);
	BCP* p_proc_bloquear = p_proc_actual;
	p_proc_bloquear->estado = BLOQUEADO;
	eliminar_primero(&lista_listos);
	insertar_ultimo(&lista_bloqueados_mutex, p_proc_bloquear);	

	p_proc_actual = planificador();
	cambio_contexto(&(p_proc_bloquear->contexto_regs), &(p_proc_actual->contexto_regs));
	fijar_nivel_int(nivel);

	}else{

		p_proc_actual->descriptores[descriptor] = &lista_mutex[mutex];
		p_proc_actual->num_descriptores++;
		lista_mutex[mutex].nombre=nombre;
		lista_mutex[mutex].estado=OCUPADO;
		lista_mutex[mutex].tipo=tipo;
		lista_mutex[mutex].num_veces_lock=0;

		total_mutex++;

		}		

	return descriptor;
}


int existe_nombre_mutex(char* nombre){
	if(total_mutex>0){
		for(int i =0; i<total_mutex;i++){
			if(strcmp(lista_mutex[i].nombre, nombre == 0))
				return 1;
		}
	}else{return -1}
}

int abrir_mutex(char *nombre){
	nombre = (char *)leer_registro(1);

	if(existe_nombre_mutex(nombre)<0)return-1;

	int descriptor = descriptores_libres();
	if(descriptor<0)return -1;
	int mutex = mutex_libres();
	if(mutex<0)return -1;

	p_proc_actual->descriptores[descriptor] = &lista_mutex[mutex];
	p_proc_actual->num_descriptores++;
	
	return descriptor;
}



int lock(unsigned int mutexid){
	mutexid = (unsigned int)leer_registro(1);
	//comprobamos si nuetro proceso contiene ese mutex
	if(p_proc_actual->descriptores[mutexid]==NULL)return -1;

	//si no ha sido nunca bloqueado
	if(p_proc_actual->descriptores[mutexid]->num_veces_lock==0){
		p_proc_actual->descriptores[mutexid]->id_propietario=p_proc_actual->id;
		p_proc_actual->descriptores[mutexid]->num_veces_lock++;
	}else{

		//si ya ha sido bloqueado mas veces
		if(p_proc_actual->descriptores[mutexid]->tipo==NO_RECURSIVO){ //NO ES RECURSIVO

			if(p_proc_actual->descriptores[mutexid]->id_propietario==p_proc_actual->id)return -1; //interbloqueo

		//BLOQUEAR PROCESO
			int nivel = fijar_nivel_int(NIVEL_3);
			p_proc_actual->estado=BLOQUEADO;
			eliminar_primero(&lista_listos);
			insertar_ultimo(&(proc_actual->descriptores[mutexid]->procesos_espera), p_proc_actual);

			BCP* p_proc_bloquear =p_proc_actual;
			p_proc_actual=planificador();

			cambio_contexto(&(p_proc_bloquear->contexto_regs), &(p_proc_actual->contexto_regs));
			fijar_nivel_int(nivel);

		}else{ //ES RECURSIVO

			if(p_proc_actual->descriptores[mutexid]->id_propietario==p_proc_actual->id){
				p_proc_actual->descriptores[mutexid]->num_veces_lock++;
			
			}else{
				//si es recursivo y no es el propietario y hay mas de un "locked", se bloquea el proceso.
				int nivel = fijar_nivel_int(NIVEL_3);
				p_proc_actual->estado=BLOQUEADO;
				eliminar_primero(&lista_listos);
				insertar_ultimo(&(proc_actual->descriptores[mutexid]->procesos_espera), p_proc_actual);

				BCP* p_proc_bloquear =p_proc_actual;
				p_proc_actual=planificador();

				cambio_contexto(&(p_proc_bloquear->contexto_regs), &(p_proc_actual->contexto_regs));
				fijar_nivel_int(nivel);
			}
		}
	}
	return 0;
}

int unlock(unsigned int mutexid){

mutexid = (unsigned int)leer_registro(1);
if(p_proc_actual->descriptores[mutexid]==NULL)return -1;
if(p_proc_actual->descriptores[mutexid]->id_propietario!=p_proc_actual->id)return -1;
if(p_proc_actual->descriptores[mutexid]->num_veces_lock==0)return -1;


if(p_proc_actual->descriptores[mutexid]->tipo==RECURSIVO){

p_proc_actual->descriptores[mutexid]->num_veces_lock--;

	if(p_proc_actual->descriptores[mutexid]->num_veces_lock==0 && p_proc_actual->descriptores[mutexid]->procesos_espera->primero!=NULL){

		int nivel = fijar_nivel_int(NIVEL_3);
		BCP* p_proc_bloqueado  = p_proc_actual->descriptores[mutexid]->procesos_espera->primero;
		p_proc_actual->estado=LISTO;
		eliminar_primero(&(p_proc_actual->descriptores[mutexid]->procesos_espera));
		insertar_ultimo(&lista_listos, p_proc_bloqueado);

		p_proc_actual->descriptores[mutexid]->id_propietario=p_proc_bloqueado->id; //¿?

		fijar_nivel_int(nivel);
	}	
}else{ //NO RECURSIVO

p_proc_actual->descriptores[mutexid]->num_veces_lock--;
if(p_proc_actual->descriptores[mutexid]->procesos_espera->primero!=NULL){


		int nivel = fijar_nivel_int(NIVEL_3);
		BCP* p_proc_bloqueado  = p_proc_actual->descriptores[mutexid]->procesos_espera->primero;
		p_proc_bloqueado->estado=LISTO;
		eliminar_primero(&(p_proc_actual->descriptores[mutexid]->procesos_espera));
		insertar_ultimo(&lista_listos, p_proc_bloqueado);

		p_proc_actual->descriptores[mutexid]->id_propietario=p_proc_bloqueado->id; //¿?

		fijar_nivel_int(nivel);
}	
	return 0;
}

int cerrar_mutex(unsigned int mutexid){
	mutexid=(unsigned int)leer_registro(1);
	if(p_proc_actual->descriptores[mutexid]==NULL)return -1;
	if(p_proc_actual->descriptores[mutexid]->id_propietario!=p_proc_actual->id)return -1;
	
	
	while(p_proc_actual->descriptores[mutexid]->procesos_espera->primero!=NULL){
		int nivel = fijar_nivel_int(NIVEL_3);
		BCP* p_proc_desbloquear = p_proc_actual->descriptores[mutexid]->procesos_espera->primero;



		for(int i=0; i<p_proc_desbloquear->num_descriptores;i++){
			if(strcmp(p_proc_desbloquear->descriptores[i]->nombre, p_proc_actual->descriptores[mutexid]->nombre == 0)){
				p_proc_desbloquear->descriptores[i]==NULL;
		}
	}
		eliminar_primero(&(p_proc_actual->descriptores[mutexid]->procesos_espera));
		insertar_ultimo(%lista_listos, p_proc_desbloquear);

		fijar_nivel_int(nivel);
	}

	//se borra el mutex del de la lista de descriptores deel proceso actual
	p_proc_actual->descriptores[mutexid]=NULL;

//desbloquear a todos los procesos que querían hacer mutex para que prueben de nuevo
	while(lista_bloqueados_mutex!=NULL){

		int nivel = fijar_nivel_int(NIVEL_3);

		BCP* p_proc_desbloquear = lista_bloqueados_mutex.primero;
		p_proc_desbloquear->estado = LISTO;
		eliminar_primero(&lista_bloqueados_mutex);
		insertar_ultimo(&lista_listos, p_proc_desbloquear);

		fijar_nivel_int(nivel);
	}

	return 0;
}

/*
 *
 * Rutina de inicialización invocada en arranque
 *
 */
int main(){
	/* se llega con las interrupciones prohibidas */

	instal_man_int(EXC_ARITM, exc_arit); 
	instal_man_int(EXC_MEM, exc_mem); 
	instal_man_int(INT_RELOJ, int_reloj); 
	instal_man_int(INT_TERMINAL, int_terminal); 
	instal_man_int(LLAM_SIS, tratar_llamsis); 
	instal_man_int(INT_SW, int_sw); 

	iniciar_cont_int();		/* inicia cont. interr. */
	iniciar_cont_reloj(TICK);	/* fija frecuencia del reloj */
	iniciar_cont_teclado();		/* inici cont. teclado */

	iniciar_tabla_proc();		/* inicia BCPs de tabla de procesos */

	/* crea proceso inicial */
	if (crear_tarea((void *)"init")<0)
		panico("no encontrado el proceso inicial");
	
	/* activa proceso inicial */
	p_proc_actual=planificador();
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	panico("S.O. reactivado inesperadamente");
	return 0;
}
