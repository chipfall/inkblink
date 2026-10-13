#include <stdio.h>
#include <string>
#include "hyphen.h"
static std::string hy(const char* w){ uint8_t p[40]; int n=hyphenPoints(w,strlen(w),p,40); std::string o; int last=0; for(int i=0;i<n;i++){ o+=std::string(w+last,p[i]-last)+"-"; last=p[i]; } o+=w+last; return o; }
int main(){
  const char* words[]={"samochód","komputer","przyjaciel","wiedźmin","zdenerwowany","ciemność","korytarz","zapach","powietrze","miasto","Polska","kostka","książka","wiedźmina","sprzedawczycy","czarodzieje","czarodziejki","poznosili","porozsiadali","rozmaitym","większość","nietaktowna","obsiadłszy","dookoła","gawędząc","wybuchając","śmiechem","przypadła","błyskawicznie","zobaczył","łagodnie","pogrążył","kołnierzu","gwałtowne","szarpnięcie","wzgórze","Nilfgaardczycy","Sodergren","Triss","uprzedzono","zamiary","wyciągają","kastetem","nakłada","cieplutkich","zdrajcy","przekupieni","przejrzano","przestrzeń","konstrukcja","nauka","poeta","mistrz","chrząszcz","szczęście","zwierzę","wykształcenie","pamiętać","opowiadanie","wiatrak","jabłko","drzewo","przedszkole","wspaniały","wszystko","rzeczywistość","spotkanie","Europa","auto","uczniowie","dziewczyna","gospodarstwo","królestwo","niebezpieczny","zajęcie","pytanie","niedźwiedź","starożytny","stanowisko","bohater","mieszkańcy","wojownik","zapytał","odpowiedział","dziewczynka","pochodnia","wędrowiec","rozmowa"};
  for(const char* w:words) printf("%s\n",hy(w).c_str());
}
