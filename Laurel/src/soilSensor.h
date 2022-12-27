#ifndef SOIL_H
#define SOIL_H

#define SOIL_RES_PIN A3
#define N_SOIL_AVG 10

int soilRes; // 3 days no water 2240, fresh watered 2800, shorted 4095 (A3 10k resistor)
int sum;
int i_ave = 0;
int readings[N_SOIL_AVG];
int init_rem = N_SOIL_AVG;


void setupSoilSensor()
{
  pinMode(SOIL_RES_PIN, INPUT);
}

int updateSoil()
{
  int newVal = analogRead(SOIL_RES_PIN);
  
  if(init_rem <= 0)
  {
    sum -= readings[i_ave];
  }
  
  readings[i_ave] = newVal;
  sum += newVal;

  i_ave++;
  i_ave %= N_SOIL_AVG;

  if(init_rem <= 0)
  {
    soilRes = sum / N_SOIL_AVG;
  }
  else 
  {
    init_rem--;
    soilRes = -1;  
  }

  return soilRes;
}

#endif
