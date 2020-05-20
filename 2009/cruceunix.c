#include <stdio.h>
#include <sys/shm.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <string.h>
#include "cruce.h"
#include <errno.h>

#define MAX 100
#define PROCESOS 1
#define ZPELIGROSA 2
#define CRUCE 3
#define P1 4 
#define P2 5 
#define C1 6
#define C2 7
#define C1AMBAR 8
#define C2AMBAR 9
#define ATROPELLO 10


void ciclo_semaforico(void);
void finaliza(int);
void waitsem(int, int);
void signalsem(int, int);
void matapadre(int);
void funcionsigterm(int);

struct global {int semid,memid,msgid,n;};
struct mensaje //Estructura del mensaje
{
	long tipo;
};
struct global GLOBAL;

#ifdef _HPUX_SOURCE
union semun 
        {int             val;
         struct semid_ds *buf;
         ushort_t        *array;
         };
#endif

int main (int argc, char * argv[])
{
	int i,ret,tipo,valor,numero,nummsg,avance,peligrosa,cruce, p1entrada, p2entrada,p1salida,p2salida,c1entrada,c1salida,c2entrada,c2salida,c1linea,c2linea,signal;
	struct posiciOn coche[6];
	struct posiciOn peaton[3];
	struct posiciOn nacimiento;
	char * puntero;
	struct sigaction sigint,sigterm;
	struct sembuf sops[2];
	struct mensaje msj;
	sigset_t padre,global;
	pid_t hijo;
	#ifdef _HPUX_SOURCE
	union semun operacion;
	#endif
	
	GLOBAL.memid = -1;
	GLOBAL.semid = -1;
	GLOBAL.msgid = -1;
	
	//INICIALIZO LA FUNCION QUE MANDARA UNA SEÑAL AL PADRE EN CASO DE ERROR
	matapadre(getpid());
	/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
					COMPROBACION DE ARGUMENTOS
	+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
	if(argc < 3)
	{
		fprintf(stderr,"Malos argumentos\n");
		matapadre(0);
		return 1;
	}
	GLOBAL.n = atoi(argv[1]);
	if(GLOBAL.n > MAX)
	{
		fprintf(stderr,"Demasiados procesos\n");
		matapadre(0);
		return 1;
	}
	ret = atoi(argv[2]);
	
	/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
					ESTABLECZO LAS MASCARAS
	+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
	//MASCARA PARA TODOS LOS PROCESOS
	if((sigfillset(&global)) == -1)
	{
		fprintf(stderr,"Error al rellenar la mascara global\n");
		return 1;
	}
	if((sigdelset(&global,SIGTERM)) == -1)
	{
		fprintf(stderr,"Error al desbloquear SIGTERM\n");
		return 1;
	}
	//MASCARA UNICAMENTE PARA EL PROCESO PADRE
	if((sigfillset(&padre)) == -1)
	{
		fprintf(stderr,"Error al rellanar la mascara padre\n");
		return 1;
	}
	if((sigdelset(&padre,SIGINT)) == -1)
	{
		fprintf(stderr,"Error al desbloquear SIGTERM\n");
		return 1;
	}
	if((sigprocmask(SIG_SETMASK,&padre,NULL)) == -1)
	{
		fprintf(stderr,"Error al establecer mascara al proceso global\n");
		return 1;
	}
	/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
					CAMBIO DE COMPORTAMIENTO DE SIGINT
	+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
	sigint.sa_handler = finaliza;
	sigint.sa_mask = global;
	sigint.sa_flags = 0;
	if((sigaction(SIGINT,&sigint,NULL)) == -1)
	{
		fprintf(stderr,"Malos argumentos\n");
		return 1;
	}
	/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
					CREACION DE MECANISMOS IPC
	+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
	GLOBAL.semid = semget(IPC_PRIVATE,11,IPC_CREAT | 0600);
	if(GLOBAL.semid == -1)
	{
		perror("Error al crear el array de semaforos\n");
		matapadre(0);
		return 1;
	}
	GLOBAL.memid = shmget(IPC_PRIVATE,256,IPC_CREAT | 0600);
	if(GLOBAL.memid == -1)
	{
		perror("Error al crear la memoria compartida\n");
		matapadre(0);
		return 1;
	}
	puntero = (char *) shmat(GLOBAL.memid,0,0);
	if((CRUCE_inicio(ret,GLOBAL.n,GLOBAL.semid,puntero)) == -1)
	{
		fprintf(stderr,"Error en CRUCE_inicio\n");
		matapadre(0);
		return 1;
	}
	GLOBAL.msgid = msgget(IPC_PRIVATE,IPC_CREAT | 0600);
	if(GLOBAL.msgid == -1)
	{
		perror("Error al crear el buzon de mensajes\n");
		matapadre(0);
		return 1;
	}
	/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
					INICIALIZACION DE LOS SEMAFOROS
	+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
	#ifdef _HPUX_SOURCE
	//Inicializo el semaforo con el numero de procesos menos dos que son el padre y el que se encarga del ciclo semaforico
	operacion.val = GLOBAL.n-2;
	if((semctl(GLOBAL.semid,PROCESOS,SETVAL,operacion)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	operacion.val = 1;
	//Inicializo el semaforo que controla la zona en la que se crean los procesos peaton a 1
	if((semctl(GLOBAL.semid,ZPELIGROSA,SETVAL,operacion)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que controla la zona en la que se cruzan las dos carreteras a 1
	if((semctl(GLOBAL.semid,CRUCE,SETVAL,operacion)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	operacion.val = GLOBAL.n-2;
	//Inicializo el semaforo que ocntrola P1
	if((semctl(GLOBAL.semid,P1,SETVAL,operacion)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	operacion.val = 0;
	//Inicializo el semaforo que ocntrola P2
	if((semctl(GLOBAL.semid,P2,SETVAL,operacion)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que ocntrola C1
	if((semctl(GLOBAL.semid,C1,SETVAL,operacion)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que ocntrola C2
	if((semctl(GLOBAL.semid,C2,SETVAL,operacion)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que ocntrola C1 el color ambar
	if((semctl(GLOBAL.semid,C1AMBAR,SETVAL,operacion)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que ocntrola C2 el color ambar
	if((semctl(GLOBAL.semid,C2AMBAR,SETVAL,operacion)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que ocntrola el cruce para pasar a la segunda fase
	if((semctl(GLOBAL.semid,ATROPELLO,SETVAL,operacion)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	#else
	//Inicializo el semaforo con el numero de procesos menos dos que son el padre y el que se encarga del ciclo semaforico
	if((semctl(GLOBAL.semid,PROCESOS,SETVAL,GLOBAL.n-2)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	if((semctl(GLOBAL.semid,PROCESOS,SETVAL,GLOBAL.n-2)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que controla la zona en la que se crean los procesos peaton a 1
	if((semctl(GLOBAL.semid,ZPELIGROSA,SETVAL,1)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que controla la zona en la que se cruzan las dos carreteras a 1
	if((semctl(GLOBAL.semid,CRUCE,SETVAL,1)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que ocntrola P1
	if((semctl(GLOBAL.semid,P1,SETVAL,GLOBAL.n-2)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que ocntrola P2
	if((semctl(GLOBAL.semid,P2,SETVAL,0)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que ocntrola C1
	if((semctl(GLOBAL.semid,C1,SETVAL,0)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que ocntrola C2
	if((semctl(GLOBAL.semid,C2,SETVAL,0)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que ocntrola C1 el color ambar
	if((semctl(GLOBAL.semid,C1AMBAR,SETVAL,0)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que ocntrola C2 el color ambar
	if((semctl(GLOBAL.semid,C2AMBAR,SETVAL,0)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	//Inicializo el semaforo que ocntrola el cruce para pasar a la segunda fase
	if((semctl(GLOBAL.semid,ATROPELLO,SETVAL,0)) == -1)
	{
		fprintf(stderr,"Error inicializando el semaforo\n");
		matapadre(0);
		return 1;
	}
	#endif
	//Rellenar el plano entero con mensajes X(-3,46) Y(1,17)
	if(pon_mensajes(-3,50,0,21) == -1)
	{
		fprintf(stderr,"Error colocnado mensajes\n");
		matapadre(0);
		return 1;
	}
	numero = 2;
	//Creo el proceso que se encarga del ciclo semaforico
	hijo = fork();//PID del proceso hijo del ciclo semaforico
	switch(hijo)
	{			
		//Creara continuamente procesos que lo controlaremos con un semaforo
		//WAIT
		//Cuando muere un hijo hay que recoger su muerte
		if(numero == GLOBAL.n)//Una vez ya haya creado todos los hijos la primera vez...
			wait(&valor);
		else
			numero++;
		case -1:
			perror("Error al crear el proceso del ciclo semaforico\n");
			matapadre(0);
			return 1;
		case 0://CICLO SEMAFORICO
			if((sigprocmask(SIG_SETMASK,&global,NULL)) == -1)
			{
				fprintf(stderr,"Error al establecer mascara al proceso global\n");
				matapadre(0);
				return 1;
			}
			ciclo_semaforico();
			return -1;
		default://EL PADRE SIGUE CREANDO HIJOS
			for(;;)
			{
				//Creara continuamente procesos que lo controlaremos con un semaforo
				//WAIT
				waitsem(PROCESOS, 1);
				//Cuando muere un hijo hay que recoger su muerte
				if(numero == GLOBAL.n)//Una vez ya haya creado todos los hijos la primera vez...
					wait(&valor);
				else
					numero++;
				avance = 0;//Inicializo la variable flag avance a 0		
				tipo = CRUCE_nuevo_proceso();
				hijo = fork();
				switch(hijo)
				{
					case -1:
						perror("Error al crear proceso hijo\n");
						matapadre(0);
						return 1;
					case 0: // Codigo del hijo
						if((sigprocmask(SIG_SETMASK,&global,NULL)) == -1)
						{
							fprintf(stderr,"Error al establecer mascara al proceso global\n");
							matapadre(0);
							return 1;
						}
						//Aqui va un coche o un peaton												oo          oo
						//REPERESENTAMOS EL COCHE COMO UN VECTOR DE POSICIONES DE HUECOS   --> 0  [ 1 | 2 | 3 | 4 ]> 5
						if(tipo == COCHE)//Movimiento coche											oo          oo
						{
							//VARIABLES PARA QUE NO ENTRE EN EL CONDICIONAL MAS DE UNA VEZ
							signal = 0;//VARIABLE QUE CONTROLA QUE SE LIBERE EL CRUCE UNA VEZ EL COCHE HAYA HECHO EL WAIT7
							//
							//VARIABLES PARA EVITAR QUE ENTRE EN LOS IFS MAS DE UNA VEZ
							//
							cruce = 0;
							peligrosa = 0;
							c1entrada = 1;
							c1salida = 1;
							c2entrada = 1;
							c2salida = 1;
							c1linea = 0;
							c2linea = 0;
							
							coche[4] = CRUCE_inicio_coche();//Nos dice la posicion de inicio
							
							for(;;)
							{
								if(coche[4].y < 0)//Significa que el coche saldra en el siguiente, de manera que liberamos las posiciones del coche
								{
									if(signal)
										signalsem(ATROPELLO,1);
									for(i=3;i>=0;i--)//LIBERO TODAS LAS POSICIONES MENOS LA SIGUIENTE
									{
										msj.tipo = coche[i].y*100+(coche[i].x+4);
										if((msgsnd(GLOBAL.msgid,&msj,sizeof(struct mensaje),0)) == -1)
										{
											perror("Error enviando el mensaje\n");
											matapadre(0);
											return 1;
										}
									}
									CRUCE_fin_coche();
									break;
								}
								msj.tipo = coche[4].y*100+(coche[4].x+4);
								if((msgrcv(GLOBAL.msgid,&msj,sizeof(struct mensaje),msj.tipo,0)) == -1)
								{
									perror("Error recibiendo un mensaje\n");
									matapadre(0);
									return 1;
								}
								//SI LA SIGUIENTE POSICION ES LA LINEA DEL CRUCE1
								if(coche[4].x == 33 && coche[4].y == 6 && c1entrada == 1)
								{
									c1entrada = 0;
									waitsem(C1,1);//COMPRUEBAS QUE SE PUEDA PASAR
									c1linea = 1;//VARIABLE QUE INDICA QUE ESTA EN LA LINEA DE CRUCE1
								}
								//SI LA SIGUIENTE POSICION ES LA LINEA DEL CRUCE2
								if(coche[4].x == 13 && coche[4].y == 10 && c2entrada == 1)
								{
									c2entrada = 0;
									signal = 1;
									//COMPRUEBAS QUE SE PUEDA PASAR
									sops[0].sem_num = C2;
									sops[0].sem_op = -1;
									sops[0].sem_flg = 0;
									//WAIT PARA EVITAR QUE PASE A LA TERCERA FASE MARCANDO QUE HAY UN COCHE EN EL CRUCE
									sops[1].sem_num = ATROPELLO;
									sops[1].sem_op = -1;
									sops[1].sem_flg = 0;
									if((semop(GLOBAL.semid,sops,2)) == -1)
									{
										perror("Error en la operacion simultanea\n");
										matapadre(0);
										return 1;
									}
									c2linea = 1;//VARIABLE QUE INDICA QUE ESTA EN LA LINEA DE CRUCE2
								}

								coche[5] = CRUCE_avanzar_coche(coche[4]);//Mueve a la posicion que le pasamos y nos dice la siguiente
								
								if(c1linea)//SI ESTA EN LA LINEA....
								{
									signalsem(C1AMBAR,1);//...PERMITO QUE SE PONGA EN AMBAR
									c1linea = 0;
								}
								if(c2linea)//SI ESTA EN LA LINEA...
								{
									signalsem(C2AMBAR,1);//...PERMITO QUE SE PONGA EN AMBAR
									c2linea = 0;
								}
								//SI EL COCHE VA A ENTRAR EN LA ZONA DEL CRUCE PELIGROSO
								if(coche[5].x > 20 && coche[5].y > 6 && cruce == 0)
								{
									waitsem(CRUCE, 1);
									cruce = 1;
								}
								//SI EL COCHE VA A SALIR DE LA ZONA DEL CRUCE PELIGROSO
								if(coche[4].y > 16 && peligrosa == 0)
								{
									signalsem(CRUCE, 1);
									peligrosa = 1;
								}
								//SI LA POSICION EN LA QUE ESTOY ESTA FUERA DE LA LINEA DEL CRUCE2
								if(coche[4].x == 29 && coche[4].y == 10 && c2salida == 1)
								{
									c2salida = 0;
									sops[0].sem_num = C2;
									sops[0].sem_op = 1;
									sops[0].sem_flg = 0;
									//SIGNAL PARA PERMITIR QUE SE PONGA EN ROJO
									sops[1].sem_num = C2AMBAR;
									sops[1].sem_op = -1;
									sops[1].sem_flg = 0;
									//WAIT PARA REESTABLECER EL VALOR DEL SEMAFORO
									if((semop(GLOBAL.semid,sops,2)) == -1)
									{
										perror("Error en la operacion simultanea\n");
										matapadre(0);
										return 1;
									}
								}
								//SI LA POSICION EN LA QUE ESTOY ESTA FUERA DE LA LINEA DEL CRUCE1
								if(coche[4].x == 33 && coche[4].y == 11 && c1salida == 1)
								{
									c1salida = 0;
									sops[0].sem_num = C1;
									sops[0].sem_op = 1;
									sops[0].sem_flg = 0;
									//SIGNAL PARA PERMITIR QUE SE PONGA EN ROJO
									sops[1].sem_num = C1AMBAR;
									sops[1].sem_op = -1;
									sops[1].sem_flg = 0;
									//WAIT PARA REESTABLECER EL VALOR DEL SEMAFORO
									if((semop(GLOBAL.semid,sops,2)) == -1)
									{
										perror("Error en la operacion simultanea\n");
										matapadre(0);
										return 1;
									}
								}
								
								if(avance > 3)
								{
									msj.tipo = coche[0].y*100+(coche[0].x+4);
									if((msgsnd(GLOBAL.msgid,&msj,sizeof(struct mensaje),0)) == -1)                

									{
										perror("Error enviando el mensaje\n");
										matapadre(0);
										return 1;
									}
								}
								//Estructura del coche en el siguiente movimiento
								coche[0] = coche[1];
								coche[1] = coche[2];
								coche[2] = coche[3];
								coche[3] = coche[4];
								coche[4] = coche[5];
								avance++;
								if((pausa_coche())== -1)
								{
									fprintf(stderr,"Error en la funcion pausa_coche\n");
									matapadre(0);
									return 1;
								}
							}
						}
						
						if(tipo == PEAToN)//Movimiento peaton
						{
							peligrosa = 1;
							p1entrada = 1;
							p2entrada = 1;
							p1salida = 1;
							p2salida = 1;
							waitsem(ZPELIGROSA, 1);//SECCION CRITICA HASTA QUE SE VAYA DE LA ZONA PELIGROSA
							//REPRESENTAMOS LOS PEATONES CON UN VECTOR DE 3 POSICIONES --> 0 (1) 2}
							peaton[1] = CRUCE_inicio_peatOn_ext(&nacimiento);//La función devolverá la posición siguiente del peatón recién creado y la posición de nacimiento en el parámetro pasado por referencia.

							msj.tipo = nacimiento.y*100+(nacimiento.x+4);

							if((msgrcv(GLOBAL.msgid,&msj,sizeof(struct mensaje),msj.tipo,0)) == -1)//... ademas de que posteriormente reservo la siguiente, tambien reservo la de nacimiento.
							{
								perror("Error recibiendo un mensaje\n");
								matapadre(0);
								return 1;
							}
							for(;;)
							{
								if(peaton[1].y < 0)//SI LA SIGUIENTE POSICION ESTA FUERA DEL PLANO
								{
									CRUCE_fin_peatOn();
									msj.tipo = peaton[0].y*100+(peaton[0].x+4);
									if((msgsnd(GLOBAL.msgid,&msj,sizeof(struct mensaje),0)) == -1)
									{
										perror("Error enviando el mensaje\n");
										matapadre(0);
										return 1;
									}
									
									break;
								}
								msj.tipo = peaton[1].y*100+(peaton[1].x+4);
								if((msgrcv(GLOBAL.msgid,&msj,sizeof(struct mensaje),msj.tipo,0)) == -1)//Aqui reservo la posicion siguiente
								{
									perror("Error recibiendo un mensaje\n");
									matapadre(0);
									return 1;
								}
								//Cuando la siguiente posicion esta en la zona del cruce P2 y no ha hecho ningun otro hace un wait
								if(peaton[1].y > 6 && peaton[1].y < 12 && peaton[1].x > 20  && peaton[1].x < 28 && p2entrada == 1)
								{
									p2entrada = 0;
									waitsem(P2, 1);
									
								}
								//Cuando la siguiente posicion esta en la zona del cruce P1 y no ha hecho ningun otro hace un wait:
								if(peaton[1].x > 29 && peaton[1].y >= 13 && peaton[1].y <= 16 && p1entrada == 1)
								{
									p1entrada = 0;
									waitsem(P1, 1);
									
								}
								///Si la siguiente posicion esta fuera del cruce P2 hace un signal y ha hecho un wait sobre P2
								if(peaton[1].y < 7 && peaton[1].x > 20  && peaton[1].x < 28 && p2salida == 1 )
								{
									p2salida = 0;
									signalsem(P2, 1);
								}
								///Si ha salido del cruce P1 hace un signal y ha hecho un wait sobre P1
								if(peaton[1].x >= 41 && peaton[1].y >= 13 && peaton[1].y <= 16 && p1salida == 1)
								{
									p1salida = 0;
									signalsem(P1, 1);
								}
								
								peaton[2] = CRUCE_avanzar_peatOn(peaton[1]);//Mueve a la posicion que le pasamos y nos dice la siguiente

								if(avance > 0)//Si ya ha habido un avance
								{
									//ENVIO MENSAJE
									msj.tipo = peaton[0].y*100+(peaton[0].x+4);
									if((msgsnd(GLOBAL.msgid,&msj,sizeof(struct mensaje),0)) == -1)//Libero la posicion anterior
									{
										perror("Error enviando el mensaje\n");
										matapadre(0);
										return 1;
									}
								}
								else//Si es el primer avance...
								{
									msj.tipo = nacimiento.y*100+(nacimiento.x+4);
									if((msgsnd(GLOBAL.msgid,&msj,sizeof(struct mensaje),0)) == -1)//...libero la posicion de nacimiento
									{
										perror("Error enviando el mensaje\n");
										matapadre(0);
										return 1;
									}
									avance++;
								}
								//CUANDO LA POSICION ACTUAL ESTE FUERA DE LA ZONA PELIGROSA HACEMOS UN SIGNAL A LA SECCION CRITICA DE LA ZONA PELGIROSA
								if(peaton[1].y < 16 && peaton[1].x > 0 && peligrosa == 1)
								{
									signalsem(ZPELIGROSA, 1);
									peligrosa = 0;
								}
								if(pausa() == -1)
								{
									fprintf(stderr,"Error en la funcion pausa\n");
									matapadre(0);
									return 1;
								}
								peaton[0] = peaton[1];
								peaton[1] = peaton[2];
							}
						}
						//LOS HIJOS AL MORIR HACEN UN SIGNAL DEL SEMAFORO DE LOS PROCESOS
						//SIGNAL
						signalsem(PROCESOS, 1);			
						return 0;
				}
			}
	}
}

void finaliza(int a)
{
	int i;
	int valor;
	struct sembuf sops;
	
	if((kill(0,SIGTERM)) == -1)
	{
		perror("Error al matar un proceso\n");
		return;
	}
	if((CRUCE_fin()) == -1)
	{
		fprintf(stderr,"Error en la funcion CRUCE_fin\n");
		return;
	}
	
	fprintf(stdout,"Procesos eliminados correctamente\n");
	fflush(stdout);
	for(i=0;i<GLOBAL.n;i++)
		wait(&valor);
	if(GLOBAL.semid != -1)
	{
		if(semctl(GLOBAL.semid,0,IPC_RMID) == -1)
		{
			perror("Error al eliminar el semaforo\n");
			return;
		}
	}
	if(GLOBAL.memid != -1)
	{
		if(shmctl(GLOBAL.memid,IPC_RMID,0) == -1)
		{
			perror("Error al eliminar la memoria compartida\n");
			return;
		}
	}
	if(GLOBAL.msgid != -1)
	{
		if(msgctl(GLOBAL.msgid,IPC_RMID,0) == -1)
		{
			perror("Error al eliminar el buzon de mensajes\n");
			return;
		}
	}
	fprintf(stdout,"Mecanismos IPCS eliminados correctamente\n");
	fflush(stdout);
	exit(0);//Termina el proceso
}

void ciclo_semaforico(void)
{
	int i,x,y;
	int bloq=0;

	//FASE CERO --> Asi viene de la tercera fase...
	if((CRUCE_pon_semAforo(SEM_C1,ROJO)) == -1)
	{
		fprintf(stderr,"Error al realizar semaforos\n");
		matapadre(0);
		return;
	}
	if((CRUCE_pon_semAforo(SEM_C2,ROJO)) == -1)
	{
		fprintf(stderr,"Error al realizar semaforos\n");
		matapadre(0);
		return;
	}
	if((CRUCE_pon_semAforo(SEM_P1,VERDE)) == -1)
	{
		fprintf(stderr,"Error al realizar semaforos\n");
		matapadre(0);
		return;
	}
	if((CRUCE_pon_semAforo(SEM_P2,ROJO)) == -1)
	{
		fprintf(stderr,"Error al realizar semaforos\n");
		matapadre(0);
		return;
	}
	for(;;)
	{
		signalsem(ATROPELLO,GLOBAL.n-2);//DESPUES DE LA TERCERA FASE DEVUELVO LOS RECURSOS QUE HE REQUERIDO PARA CAMBIAR A ESA FASE
		//PRIMERA FASE
		//Bloqueo P1 (N--> 0) Antes de que cambie de color
		waitsem(P1, GLOBAL.n-2);//ESPERA A QUE LOS PEATONES PASEN EL CRUCE Y LUEGO IMPIDE QUE PASEN EN ROJO
		if((CRUCE_pon_semAforo(SEM_P1,ROJO)) == -1)
		{
			fprintf(stderr,"Error al realizar semaforos\n");
			matapadre(0);
			return;
		}
		if((CRUCE_pon_semAforo(SEM_C1,VERDE)) == -1)
		{
			fprintf(stderr,"Error al realizar semaforos\n");
			matapadre(0);
			return;
		}
		signalsem(C1AMBAR,GLOBAL.n-2);
		signalsem(C1,1);//PERMITE QUE SE PASE
		if((CRUCE_pon_semAforo(SEM_P2,VERDE)) == -1)
		{
			fprintf(stderr,"Error al realizar semaforos\n");
			matapadre(0);
			return;
		}
		//Desbloqueo P2 0 --> N
		signalsem(P2, GLOBAL.n-2);//PERMITO QUE PASEN LOS PEATONES
		for(i=0;i<6;i++)
		{
			if((pausa()) == -1)
			{
				fprintf(stderr,"Error en la pausa\n");
				matapadre(0);
				return;
			}
		}
		//SEGUNDA FASE
		//Bloqueo P2 N --> 0
		waitsem(P2, GLOBAL.n-2);
		if((CRUCE_pon_semAforo(SEM_P2,ROJO)) == -1)
		{
			fprintf(stderr,"Error al realizar semaforos\n");
			matapadre(0);
			return;
		}
		waitsem(C1,1);//IMPIDO QUE PASEN MAS COCHES SI HAY, DEJANDO PASAR EL QUE HA PISADO YA LA LINEA
		if((CRUCE_pon_semAforo(SEM_C1,AMARILLO)) == -1)
		{
			fprintf(stderr,"Error al realizar semaforos\n");
			matapadre(0);
			return;
		}
		for(i=0;i<2;i++)
		{
			if((pausa()) == -1)
			{
				fprintf(stderr,"Error en la pausa\n");
				matapadre(0);
				return;
			}
		}
		waitsem(C1AMBAR,GLOBAL.n-2);//ESPERO A QUE EL COCHE HAYA PASADO PARA PONERLO EN ROJO
		if((CRUCE_pon_semAforo(SEM_C1,ROJO)) == -1)
		{
			fprintf(stderr,"Error al realizar semaforos\n");
			matapadre(0);
			return;
		}
		if((CRUCE_pon_semAforo(SEM_C2,VERDE)) == -1)
		{
			fprintf(stderr,"Error al realizar semaforos\n");
			matapadre(0);
			return;
		}
		signalsem(C2AMBAR,GLOBAL.n-2);
		signalsem(C2,1);
		if((CRUCE_pon_semAforo(SEM_P1,ROJO)) == -1)
		{
			fprintf(stderr,"Error al realizar semaforos\n");
			matapadre(0);
			return;
		}
		
		for(i=0;i<8;i++)
		{
			if((pausa()) == -1)
			{
				fprintf(stderr,"Error en la pausa\n");
				matapadre(0);
				return;
			}
		}
		//TERCERA FASE
		//ANTES DE PASAR A LA TERCERA FASE NECESITO RESERVAR EL CRUCE ANTES DE CAMBIAR LOS SEMAFOROS
		if((CRUCE_pon_semAforo(SEM_C1,ROJO)) == -1)
		{
			fprintf(stderr,"Error al realizar semaforos\n");
			matapadre(0);
			return;
		}
		waitsem(C2,1);
		if((CRUCE_pon_semAforo(SEM_C2,AMARILLO)) == -1)
		{
			fprintf(stderr,"Error al realizar semaforos\n");
			matapadre(0);
			return;
		}
		for(i=0;i<2;i++)
		{
			if((pausa()) == -1)
			{
				fprintf(stderr,"Error en la pausa\n");
				matapadre(0);
				return;
			}
		}
		waitsem(C2AMBAR,GLOBAL.n-2);
		if((CRUCE_pon_semAforo(SEM_C2,ROJO)) == -1)
		{
			fprintf(stderr,"Error al realizar semaforos\n");
			matapadre(0);
			return;
		}
		//fprintf(stderr,"GESTOR: SIGNAL P1(ROJO --> VERDE)\n");fflush(stderr);
		if((CRUCE_pon_semAforo(SEM_P2,ROJO)) == -1)
		{
			fprintf(stderr,"Error al realizar semaforos\n");
			matapadre(0);
			return;
		}
		//ANTES DE POENR EL SEMAFORO DE LOS PEATONES A VERDE HAY QUE COMPROBAR QUE NO HAYA NADIE EN EL CRUCE
		waitsem(ATROPELLO,GLOBAL.n-2);
		if((CRUCE_pon_semAforo(SEM_P1,VERDE)) == -1)
		{
			fprintf(stderr,"Error al realizar semaforos\n");
			matapadre(0);
			return;
		}
		//Desbloqueo P1 0 --> N
		signalsem(P1, GLOBAL.n-2);
		//LIBERO EL CRUCE
		for(i=0;i<12;i++)
		{
			if((pausa()) == -1)
			{
				fprintf(stderr,"Error en la pausa\n");
				matapadre(0);
				return;
			}
		}
		bloq=1;
	}
	return;
}

void waitsem(int semaforo, int num)
{
	struct sembuf sops;
	sops.sem_num = semaforo;
	sops.sem_op = -num;
	sops.sem_flg = 0;
	if((semop(GLOBAL.semid,&sops,1)) == -1)
	{
		fprintf(stderr,"Error en el wait\n");
		matapadre(0);
		exit(1);
	}
}

void signalsem(int semaforo, int num)
{
	struct sembuf sops;
	sops.sem_num = semaforo;
	sops.sem_op = num;
	sops.sem_flg = 0;
	if((semop(GLOBAL.semid,&sops,1)) == -1)
	{
		fprintf(stderr,"Error en el wait\n");
		matapadre(0);
		exit(1);
	}
}


int pon_mensajes(int x1,int x2,int y1,int y2)
{
	int x,y;
	long tipo;
	struct mensaje msj;
	for(x=x1;x<=x2;x++)
	{
		for(y=y1;y<=y2;y++)
		{
			msj.tipo = y*100+(x+4);
			if((msgsnd(GLOBAL.msgid,&msj,sizeof(struct mensaje),0)) == -1)
			{
				perror("Error al enviar el mensaje\n");
				matapadre(0);
				return -1;
			}
		}
	}
}

void matapadre(int a)
{
	static int padre;
	static int i = 0;
	
	if(i == 0)
	{
		padre = a;
	}
	else
	{
		if((kill(padre,SIGINT)) == -1)
		{
			perror("Error al enviar un SIGINT al padre debido a un error\n");
			matapadre(0);
			return;
		}
	}
}

