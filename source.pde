// Zombie Infection Simulation
// Kevan Davis, 16/8/03
// Modified by John Gilbertson 25/09/03
// just making zombies more dangerous, edited level creation code, increased max zombies, made zombies less likely to group etc...

int freeze;
int num=4000;
int speed=1;
int panic=5;
int human;
int panichuman;
int zombie;
int dead;
int wall;
int empty;
int clock=0;
int hit;
int ishuman=2;
int ispanichuman=4;
int iszombie=1;
int isdead=5;

Being[] beings;

void setup() 
{ 
  BFont f=loadFont("Univers65.vlw.gz");
  framerate(45);
  textFont(f,19);
  
  int x,y; 
  size(400,200); 
  human=color(200,0,200);
  panichuman=color(255,120,255);
  zombie=color(0,255,0);
  wall=color(50,50,60);
  empty=color(0,0,0);
  dead=color(128,30,30);
  hit=color(128,128,0);
  clock=0;
//  noBackground(); 

  fill(wall);
  rect(0,0,width,height); 
  fill(wall);
  stroke(empty);
  for (int i=0; i<100; i++)
  {
      rect((int)random(width)-(width/8), (int)random(height)-(height/8),(int)random(width/4), (int)random(height/4)); 
  }
 
  fill(empty);
  stroke(empty);
  for (int i=0; i<45; i++)
  {
    rect((int)random(width-1)+1, (int)random(height-1)+1, (int)random(40)+10,(int)random(40)+10); 

    x=(int)random(width-1)+1;
    y=(int)random(height-1)+1;
  }

/*  fill(wall);
  stroke(wall);
  rect(190,0,20,200);
  rect(0,90,400,20);*/

  fill(wall);
  noStroke();
  rect(0,height-30,width,height);
  fill(empty);
  stroke(wall);
  rect(0,height-30,width-2,height-2);
  beings = new Being[8000];        
  for(int i=0; i<num; i++)
  { beings[i] = new Being(); beings[i].position(); }

  beings[0].infect();
  beings[1].infect();
  beings[2].infect();
  beings[3].infect();
  freeze=0;
} 
 

void loop() 
{ 
  if (freeze==0)
  {
//    background(empty);
    fill(wall);
    noStroke();
    rect(0,height-30,width,height);
    fill(empty);
    stroke(wall);
    rect(0,height-30,width-2,height-2);  
    
    for(int i=0; i<num; i++) 
    { 
      beings[i].move(); 
    } 
/*    if (speed==2) { delay(20); }
    else if (speed==3) { delay(50); }
    else if (speed==4) { delay(100); }*/
    clock++;
    int numZombies=0;
    int numDead=0;
        
    for(int j=0;j<num;j++)
    {
      if(beings[j].type==iszombie)
      {
        numZombies++;
      }
      if(beings[j].type==isdead)
      {
        numDead++;
      }
    }
    fill(human);
    noStroke();
    String s="Humans:" + (num-(numDead+numZombies));
    text(s,2,height-10);
    
    fill(zombie);
    String s2="Zombies: " + numZombies;
    text(s2,width-120,height-10);
    
    fill(dead);
    String s3="Dead: " + numDead;
    text(s3,120,height-10);
  }
}

void mousePressed()
{
  int mx=mouseX;
  int my=mouseY;
  int radius=(int)random(10)+6;
  fill(empty);
  stroke(hit);
  ellipse(mx-radius,my-radius,radius*2,radius*2);
  for(int i=0;i<num;i++)
  {
    int dx=beings[i].xpos-mx;
    int dy=beings[i].ypos-my;
    int diff=(dx*dx)+(dy*dy);
    if(diff<(radius*radius))
    {
      beings[i].die();
    }
  }
}

int look(int x, int y,int d,int dist)
{
  for(int i=0; i<dist; i++)
  {
    if (d==1) { y--; }
    if (d==2) { x++; }
    if (d==3) { y++; }
    if (d==4) { x--; }

    if (x>width-1 || x<1 || y>height-30 || y<1)
    { return 3; }
    else if (get(x,y) == wall)
    { return 3; }
    else if (get(x,y) == panichuman)
    { return 4; }
    else if (get(x,y) == human)
    { return 2; }
    else if (get(x,y) == zombie)
    { return 1; }
    else if (get(x,y) == dead)
    { return 0; }
  }
  return 0;
}

void keyPressed() 
{ 
  if(key == ' ')
  {
    key='z';
  } 
  if(key == '+' && num < 8000) { num += 100; key='z'; }
  if(key == '-' && num > 4000) { num -= 100; key='z'; }
  if(key == 'z' || key == 'Z')
  {
    freeze=1;
    setup(); 
  }
}

class Being
{ 
  int xpos, ypos, dir;
  int type, active;
  int belief, maxBelief;
  int zombielife;
  int rest;

  Being()
  {
    dir = (int)random(4)+1;
    type = ishuman;
    active = 0;
    belief=500;
    maxBelief=500;
    zombielife=1300;
    rest=40;
  }

  void die()
  {
    type=5;
  }

  void position()
  {
    for (int ok=0; ok<100; ok++)
    {
      xpos = (int)random(width-1)+1; 
      ypos = (int)random(height-30)+1;
      if (get(xpos,ypos)==color(0,0,0)) { ok = 100; }
    }
  }

  void infect(int x, int y)
  {
    if (x==xpos && y==ypos)
    { 
      type = 1; 
    }
  }

  void infect()
  { type = 1; set(xpos, ypos, zombie); }

  void uninfect()
  { 
    type = ishuman; 
    active = 0;    
    zombielife=1000;
    belief=500;
  }

  void move()
  {
    if(type==5)
    {
      set(xpos,ypos,dead);
    }
    else
    {
      int r = (int)random(3);
      if ((type==ishuman && active>0) || r==1)
      {
        set(xpos, ypos, color(0,0,0));
        
        if(belief<=0)
        {
          active=0;
          maxBelief-=100;
          belief=0;
          rest=0;
        }
            
        if (look(xpos,ypos,dir,1)==0)
        {
          if (dir==1) { ypos--; }
          else if (dir==2) { xpos++; }
          else if (dir==3) { ypos++; }
          else if (dir==4) { xpos--; }
/*          if(belief>0)
          {
            belief--;
          }*/
        }
        else
        {
          dir = (int)random(4)+1; 
        }

        if (type == 1)
        { set(xpos, ypos, zombie); }
        else if (active > 0)
        { set(xpos, ypos, panichuman); }
        else
        { set(xpos, ypos, human); }
        if (active>0) {active--;}
      }

      int target = look(xpos,ypos,dir,10);

      if (type==1)
      {
        zombielife--;
        if (target==2 || target==4) { active = 10;}
        else if (target==3)
        {
          if((int)random(8)==1)
          {
            if (dir==1) { set(xpos,ypos-1,empty); }
            else if (dir==2) { set(xpos+1,ypos,empty); }
            else if (dir==3) { set(xpos,ypos+1,empty); }
            else if (dir==4) { set(xpos-1,ypos,empty); }

          }
          if(look(xpos,ypos,dir,2)==iszombie)
          {
            dir=dir+1;
            if(dir>4) dir=1;
          }
        
        }

        else if (active==0 && target!=1) 
        { 
          if((int)random(6)>4)
          {
            if((int)random(2)==0)
              dir = dir+1;
            else
              dir = dir-1;
            if(dir==5)
              dir=1;
            if(dir==0)
              dir=4;
          }
          else
          {
            dir=dir;
          }
       
        }
 
        int victim = look(xpos,ypos,dir,1);
        if (victim == 2 || victim==4)
        {
          int ix = xpos; int iy = ypos;
          if (dir==1) { iy--; }
          if (dir==2) { ix++; }
          if (dir==3) { iy++; }
          if (dir==4) { ix--; }
          for(int i=0; i<num; i++)
          { 
            beings[i].infect(ix,iy); 
          }
        }  
        if(zombielife<=0)
        {
          die();
        }
      }
      else if (type==2)
      {
        if (target==1)
        {
          maxBelief=500;
          belief=500;
          active=10;
          rest=0;
        }
        if(target==4)
        { 
          active=10;
          if(belief>0)
          {
            belief--;
          }
          else if(belief==0 && maxBelief!=0 && rest>=40)
          {
            belief=maxBelief;
            rest=0;
          }
          else if(belief==0 && rest<40)
          {
            rest++;
          }
        }
        else if(target!=1 && target!=4 && active==0)
        {
          rest++;
        }
        else if (target==1)
        {       
          dir = dir + 2; if (dir>4) { dir = dir - 4; } 
        }
        if ((int)random(2)==1) { dir = (int)random(4)+1; }
      }
    }
  }
}
