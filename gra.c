#include<sys/types.h>
#include<sys/socket.h>
#include<stdio.h>
#include<ctype.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<stdlib.h>
#include<netdb.h>
#include<string.h>
#include<errno.h>

//struktura przesyłanej wiadomości
struct my_msg
{
    char name[16];
    char text[10];
};

u_short my_port = 5444;
int sockfd;
int planszaINT[4][4]={{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0}}; //Plansza jako inty 0 -nie strzelano, 1 -pudło , 2 -zatopienie , 3 -trafienie
int jma1; //jednomasztowiec1 jako int
int jma2; //jednomasztowiec2 jako int
int d1; //dwumasztowiec1 jako int
int d2; //dwumasztowiec2 jako int
int fl=0; //jednokrotne przepisanie nicku gracza
int wygrane=0; //flaga do konczenia rozgrywki
int pytaj=0; //1 -wyszedlem
int trafienia=0;
char wrogidm[3]="D5"; //zapis adresu pierwszego trafionego "masztu" dwumasztowca
char nickwroga[16];
char source[16]; //ip wroga
struct addrinfo *gracz2; //dane przeciwnika
struct addrinfo *result; //moje dane
struct addrinfo *rp;
struct my_msg mojawiadomosc;
struct my_msg wrogawiadomosc;

void uruchom(char* arg1);
void serwer();
void klient();
void ustaw();
int sprawdz(char* dm1,char* dm2, int opcja);
void wypisz();
int miejscenaplanszy(char* pole);
int trafienie(char* pole);
void wygrana();
void aktualizuj(char* pole, int wartosc);
int strzal(char *pole);
void koniec();
void wyslij();
int reaguj();
void czysc();

int main(int argc, char *argv[])
{
	char *pow="Walczmy";
	char opcja='n';
	int flag=0;

	if(argc<2 || argc>3)
	{
		printf("Podales/las bledna ilosc argumentow. Podaj adres/nazwe przeciwnika oraz nick(opcjonalnie).\n");
		return 0;
	}

	//Przypisanie odpowiedniego nicku
	if(argc==3)
	{
		strcpy(mojawiadomosc.name,argv[2]);
	}
	else
	{
		strcpy(mojawiadomosc.name,"NN");
	}
	uruchom(argv[1]);
	//glowna petla odpowiedzialna za wlasciwe elementy gry
	do
	{
		flag=0;
		//przygotowanie planszy i czyszczenie niezbednych zmiennych
		czysc();
		ustaw();
		//wyslanie propozycji gry
		strcpy(mojawiadomosc.text,pow);
		klient();
		//petla odpowiedzialna za komunikacje miedzy graczami
		while(1)
		{
			serwer();
			if(trafienia==4 || wygrane==1)
			{
				break;
			}
		}
		//opcjonalne pobieranie informacji o ponownej rozgrywce
		if(pytaj==0)
		{
			while(1)
			{
				opcja = getchar();
				opcja = getchar();
				if(opcja=='t')
				{
					flag=1;
					break;
				}
				else if(opcja=='n')
				{
					break;
				}
				else
				{
					printf("Wybrales/las niepoprawna opcje\n");
				}
			}
		}
	}
	while(flag!=0);
	result=NULL;
	freeaddrinfo(gracz2);
	gracz2=NULL;
	freeaddrinfo(rp);
	rp=NULL;
	close(sockfd);
	return 0;
}

void uruchom(char* arg1)
{
	int s;
	int s2;
	struct addrinfo hints;

	memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;

        //zdobywanie informacji o obu graczach przy wykorzytsaniu getaddrinfo
	s2 = getaddrinfo(NULL,"5444", &hints, &result); //do bind

        hints.ai_flags = 0;
        s = getaddrinfo(arg1,"5444", &hints, &gracz2); //do sendto

        if (s != 0)
        {
               fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
               exit(EXIT_FAILURE);
        }
        if (s2 != 0)
        {
               fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s2));
               exit(EXIT_FAILURE);
        }

	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sockfd == -1)
		{
			perror("Blad socket: ");
			continue;
		}

		if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == 0)
		{
			break;
		}

		close(sockfd);
	}

	inet_ntop(AF_INET, &(((struct sockaddr_in *)gracz2->ai_addr)->sin_addr), source,16);
	printf("Rozpoczynam gre z %s. Napisz <koniec> aby zakonczyc.", source);
}

//odbieranie wiadomosci od drugiego gracza
void serwer()
{
	recvfrom(sockfd, &wrogawiadomosc, sizeof(wrogawiadomosc), 0, NULL, NULL);
	if(fl==0)
	{
		strcpy(nickwroga,wrogawiadomosc.name);
		fl++;
	}
	reaguj();
}

//wysylanie wiadomosci do drugiego gracza
void klient()
{
	ssize_t bytes;
	bytes = sendto(sockfd, &mojawiadomosc, sizeof(mojawiadomosc),0,gracz2->ai_addr, gracz2->ai_addrlen);

	if(bytes < 0)
	{
		perror("Blad sendto: ");
	}
	else if(bytes > 0 && strcmp("Walczmy",mojawiadomosc.text)==0)
	{
		printf("[Propozycja gry wyslana]\n");
	}
}

//Ustawianie okrętów na planszy
void ustaw()
{
	int flaga=0;
	int blad;
	int blad1;
	int blad2;
	int blad3;
	int blad4;
	char jm1[3];
	char jm2[3];
	char dm[3];
	char dm1[3];

	printf(" Ustal polozenie swoich okretow:\n");

	//Ustawienie pierwszego okrętu
	while(flaga==0)
	{
		printf("1. jednomasztowiec: ");
		scanf("%s",jm1);
		jm1[0]=toupper(jm1[0]);
		jm1[1]=toupper(jm1[1]);
		//sprawdzanie potencjalnych bledow
		if(strlen(jm1)!=2)
		{
			printf("To nie jest poprawne pole \n");
		}
		else if(sprawdz(jm1,jm1,0)==2)
		{
			printf("Wyszedles/las poza plansze \n");
		}
		else
		{
			flaga=1;
		}
	}

	//Ustawienie drugiego okrętu
	while(flaga==1)
	{
		printf("2. jednomasztowiec: ");
		scanf("%s",jm2);
		jm2[0]=toupper(jm2[0]);
		jm2[1]=toupper(jm2[1]);
		blad=sprawdz(jm1,jm2,0);
		//sprawdzanie potencjalnych bledow
		if(strncmp(jm1,jm2,2)==0)
		{
			printf("To miejsce jest juz zajete, podaj inne\n");
		}
		else if(blad==2)
		{
			printf("Wyszedles/las poza plansze \n");
		}
		else if(blad==0)
		{
			printf("Statki nie moga ze soba sasiadowac\n");
		}
		else if(strlen(jm2)!=2)
		{
			printf("To nie jest poprawne pole \n");
		}
		else
		{
			flaga=0;
		}
	}

	//Ustawienie dwumasztowca
	while(flaga==0)
	{
		printf("3. dwumasztowiec: ");
		scanf("%s %s",dm,dm1);
		dm[0]=toupper(dm[0]);
		dm[1]=toupper(dm[1]);
		dm1[0]=toupper(dm1[0]);
		dm1[1]=toupper(dm1[1]);

		blad=sprawdz(dm,dm1,1);
		blad1=sprawdz(jm1,dm1,0);
		blad2=sprawdz(jm1,dm,0);
		blad3=sprawdz(jm2,dm1,0);
		blad4=sprawdz(jm2,dm,0);
		//sprawdzanie potencjalnych bledow
		if(strlen(dm)!=2 || strlen(dm1)!=2)
		{
			printf("To nie jest poprawne pole \n");
		}
		else if(strncmp(jm1,dm,2)==0 || strncmp(jm1,dm1,2)==0 || strncmp(jm2,dm,2)==0 || strncmp(jm2,dm1,2)==0)
		{
			printf("To miejsce jest juz zajete, podaj inne\n");
		}
		else if(blad==1)
		{
			printf("Dwumasztowiec musi stac na sasiadujacych polach\n");
		}
		else if(blad1==0 || blad2==0 || blad3==0 || blad4==0)
		{
			printf("Statki nie moga ze soba sasiadowac\n");
		}
		else if(blad==2)
		{
			printf("Wyszedles/las poza plansze \n");
		}
		else
		{
			flaga=1;
		}
	}

	//Umieszczenie współrzędnych przeliczonych na int do zmiennych globalnych
	jma1=miejscenaplanszy(jm1);
	jma2=miejscenaplanszy(jm2);
	d1=miejscenaplanszy(dm);
	d2=miejscenaplanszy(dm1);
}

//Sprawdzanie poprawności umieszczenia statku na podanym polu
int sprawdz(char* dm1,char* dm2, int opcja)
{
	// 0 -sąsiadują
	// 1 -nie sąsiadują
	// 2 -poza planszą
	int polozenie=0;
	int polozenie2=0;
	int roznica;

	//Sprawdzenie czy podane pole znajduje się na planszy
	if((polozenie=miejscenaplanszy(dm1))==-2)
	{
		return 2;
	}
	if((polozenie2=miejscenaplanszy(dm2))==-2)
	{
		return 2;
	}
	if(polozenie2>polozenie)
	{
		int pom = polozenie;
		polozenie=polozenie2;
		polozenie2=pom;
	}
	roznica=polozenie-polozenie2;

	//Sprawdzanie sasiedztwa
	if(roznica==4)
	{
		return 0;
	}
	else if(polozenie%4!=1 && roznica==1)
	{
		return 0;
	}
	else if(polozenie2%4==0)
	{
		return 1;
	}

	//Sprawdzanie sasiedztwa "na rogach" statkow
	if(opcja==0)
	{
		if(polozenie%4==0 && roznica==5)
		{
			return 0;
		}
		else if(polozenie%4==1 && roznica==3)
		{
			return 0;
		}
		else if(polozenie%4!=1 && polozenie%4!=0 && (roznica==3 || roznica==5) )
		{
			return 0;
		}
	}

	return 1;
}

//Wypisywanie aktualnej planszy gry
void wypisz()
{
	int i=0;
	int j=0;
	char tab[5]={'A','B','C','D'};
	char znaki[5]={' ','x','Z','t'};
	//Wypisywanie współrzędnych
	printf("  1  2  3  4 \n");
	for(i=0;i<4;i++)
	{
		printf("%c",tab[i]);
		//Wypisywanie właściwych pól
		for(j=0;j<4;j++)
		{
			printf(" %c ",znaki[planszaINT[i][j]]);
		}
		printf("\n");
	}
}

//Przeliczanie podanego pola na wartośc w int
int miejscenaplanszy(char* pole)
{
	int miejsce;
	int i=0;
	char p1[5]={'A','B','C','D'};
	char p2[5]={'1','2','3','4'};
	//Sprawdzanie pierwszej wspólrzędnej
	for(i=0;i<4;i++)
	{
		if((toupper(pole[0])==p1[i]))
		{
			miejsce=4*i;
			i=9;
		}
	}
	if(i!=10)
	{
		return -2;
	}
	//Sprawdzanie drugiej wspólrzędnej
	for(i=0;i<4;i++)
	{
		if(toupper(pole[1])==p2[i])
		{
			miejsce+=(i+1);
			i=9;
		}
	}
	if(i!=10)
	{
		return -2;
	}

	return miejsce;
}

//sprawdzam czy trafiłem ktoryś statek
int trafienie(char* pole)
{
	//0 - pudło
	//1 - trafienie(zatopienie) jednomasztowca
	//2 - zatopienie dwumasztowca
	//3 - trafienie dwumasztowca
	int traf;
	int flaga=0;

	//sprawdzam czy nastapilo juz trafienie dwumasztowca
	if(d1==-1 || d2==-1)
	{
		flaga=1;
	}

	traf=miejscenaplanszy(pole);

	//sprawdzam czy nastąpiło trafienie
	if(traf==jma1)
	{
		jma1=-1;
		return 1;
	}
	else if(traf==jma2)
	{
		jma2=-1;
		return 1;
	}
	else if(traf==d1)
	{
		d1=-1;
		if(flaga==1)
		{
			return 2;
		}

		return 3;
	}
	else if(traf==d2)
	{
		d2=-1;
		if(flaga==1)
		{
			return 2;
		}

		return 3;
	}

	return 0;
}

//Sprawdzanie czy udało się nam już wygrać, 4 trafienia oznaczają zatopienie wszystkich statków wroga
void wygrana()
{
	strcpy(mojawiadomosc.text,"wygralem");
	klient();
	printf("\n[Wygrales, czy chcesz przygotowac nowa plansze? (t/n)]\n");
}

//aktualizacja planszy
void aktualizuj(char* pole, int wartosc)
{
	int traf;
	int i;
	int j;
	pole[0]=toupper(pole[0]);
	pole[1]=toupper(pole[1]);
	traf=miejscenaplanszy(pole);

	i=(traf-1)/4;
	j=(traf-(i*4))-1;

	planszaINT[i][j]=wartosc;

	//aktualizacaja pola z trafionym dwumasztowcem
	if(strcmp(wrogidm,"D5")!=0)
	{
		traf=miejscenaplanszy(wrogidm);
		i=(traf-1)/4;
		j=(traf-(i*4))-1;
		planszaINT[i][j]=2;
	}

	if(wartosc==3)
	{
		strcpy(wrogidm,pole);
	}
}

//sprawdzanie czy oddano juz strzal na zadane pole
int strzal(char *pole)
{
	//0 -mozna oddac strzal
	//1 -juz oddano strzal na te pole
	int i;
	int j;
	int traf;
	traf=miejscenaplanszy(pole);

	i=(traf-1)/4;
	j=(traf-(i*4))-1;

	if(planszaINT[i][j]!=0)
	{
		return 1;
	}

	return 0;
}

// funkcja po wpisaniu <koniec>
void koniec()
{
	//wyslanie odpowiedniej wiadomosci
	strcpy(mojawiadomosc.text,"Koniec");
	klient();
	pytaj=1;
	wygrane=1;
	printf("[Koncze gre, zapraszam ponownie]\n");

	//Sprzatanie
	result=NULL;
	freeaddrinfo(gracz2);
	gracz2=NULL;
	freeaddrinfo(rp);
	rp=NULL;
	close(sockfd);
}

//wysylanie wiadomosci
void wyslij()
{
	while(1)
	{
		scanf("%s",mojawiadomosc.text);
		//Sprawdzanie poprawnosci wiadomosci oraz potencjalna inicjacja funkcji za wpisaniu odpowiedniej wiadomosci
		if(strcmp(mojawiadomosc.text,"<koniec>")==0)
		{
			koniec();
			break;
		}
		else if(strcmp(mojawiadomosc.text,"wypisz")==0)
		{
			wypisz();
		}
		else if(sprawdz(mojawiadomosc.text,mojawiadomosc.text,1)==2)
		{
			printf("Te koordynaty znajduja sie poza plansza\n");
		}
		else if(strlen(mojawiadomosc.text)!=2)
		{
			printf("Niepoprawne dane, prosze podac koordynaty w postaci np. \"A1\", \"C2\"\n");
		}
		else if(strzal(mojawiadomosc.text)==1)
		{
			printf("Strzelales/las juz w to miejsce\n");
		}
		else
		{
			klient();
			break;
		}
	}
}

//funkcja odpowiedzialna za reagowanie zalezne od wiadomosci
int reaguj()
{
	//0 -ja strzelam
	//1 -gra skonczona
	//2 -strzela wrog
	int strzal;
	printf("[");
	//Sprawdzanie wiadomosci wroga
	if(strcmp(wrogawiadomosc.text,"Walczmy")==0)
	{
		printf("%s (%s), dolaczyl do gry, podaj pole do strzalu]\n",wrogawiadomosc.name,source);
	}
	else if(strcmp(wrogawiadomosc.text,"Koniec")==0)
	{
		printf("%s (%s), zakonczyl gre, czy chcesz przygotowac nowa plansze? (t/n)]\n",nickwroga,source);
		wygrane=1;
		return 1;
	}
	else if(strcmp(wrogawiadomosc.text,"wygralem")==0)
	{
		wygrane=1;
		printf("Niestety przegrales, czy chcesz przygotowac nowa plansze? (t/n)]\n");
		return 1;
	}
	else if(strcmp(wrogawiadomosc.text,"p0")==0)
	{
		printf("Pudlo, ");
		aktualizuj(mojawiadomosc.text,1);
		return 2;
	}
	else if(strcmp(wrogawiadomosc.text,"t1")==0)
	{
	 	printf("%s (%s) : zatopiles jednomasztowiec, podaj kolejne pole]\n", nickwroga, source);
	 	aktualizuj(mojawiadomosc.text,2);
	 	trafienia++;
	}
	else if(strcmp(wrogawiadomosc.text,"z2")==0)
	{
		printf("%s (%s) : zatopiles dwumasztowiec, podaj kolejne pole] \n", nickwroga, source);
		aktualizuj(mojawiadomosc.text,2);
		trafienia++;
	}
	else if(strcmp(wrogawiadomosc.text,"t2")==0)
	{
		printf("%s (%s) : trafiles dwunomasztowiec, podaj kolejne pole] \n", nickwroga, source);
		aktualizuj(mojawiadomosc.text,3);
		trafienia++;
	}
	else
	{
		//w przypadku gdy wiadomosc wroga nie byla zadna ze "specjalnych" wiadomosci
		printf("%s (%s) strzela %s - ", nickwroga, source, wrogawiadomosc.text);
		strzal=trafienie(wrogawiadomosc.text);

		if(strzal==0)
		{
			printf("pudlo, podaj pole do strzalu]\n");
			strcpy(mojawiadomosc.text,"p0");
			klient();
			wyslij();
			return 0;
		}
		else if(strzal==1)
		{
			printf("jednomasztowiec zatopiony]\n");
			strcpy(mojawiadomosc.text,"t1");
		}
		else if(strzal==2)
		{
			printf("dwumasztowiec zatopiony]\n");
			strcpy(mojawiadomosc.text,"z2");
		}
		else if(strzal==3)
		{
			printf("dwumasztowiec trafiony]\n");
			strcpy(mojawiadomosc.text,"t2");
		}
		klient();
		return 2;
	}
	if(trafienia!=4)
	{
		wyslij();
	}
	else
	{
		wygrana();
	}
	return 0;
}

//czyszczenie niezbednych zmiennych globalnych przed rozpoczeciem rozgrywki
void czysc()
{
	int i=0;
	int j=0;
	fl=0;
	trafienia=0;
	strcpy(wrogidm,"D5");
	wygrane=0;
	for(i=0;i<4;i++)
	{
		for(j=0;j<4;j++)
		{
			planszaINT[i][j]=0;
		}
	}
}
