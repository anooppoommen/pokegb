#include <SDL2/SDL.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

using u8=uint8_t; using s8=int8_t; using u16=uint16_t; using u32=uint32_t; using u64=uint64_t;
u8 op, u, v, fc, *rom0, *rom1, io[512], vram[8192], wram[16384], *eram, *eram1,
    R[] = {19, 0, 216, 0, 77, 1, 176, 1, 254, 255}, &E = R[2], &D = R[3],
    &F = R[6], &A = R[7], *R8[] = {&R[1], &R[0], &D, &E, &R[5], &R[4], &F, &A},
    &IF = io[271], &LCDC = io[320], &LY = io[324], &WY = io[330], &WX = io[331],
    &IE = io[511], IME = 0, halt = 0;
u8 const *Sk;
u16 PC = 256, U, &HL = (u16 &)R[4], &SP = (u16 &)R[8],
    *R16[] = {(u16 *)&R[0], (u16 *)&R[2], &HL, &SP},
    *R162[] = {(u16 *)&R[0], (u16 *)&R[2], &HL, (u16 *)&R[6]},
    &DIV = (u16 &)io[259], dot = 32;
u32 fb[23040], pal[4]={0xffffffff,0xffaaaaaa,0xff555555,0xff000000};
u64 bcy, cy = 0;

u8 r(u16 a) { cy+=4;
  switch (a >> 12) {
    default: case 0 ... 3: return rom0[a];
    case 4 ... 7: return rom1[a & 16383];
    case 8 ... 9: return vram[a & 8191];
    case 10 ... 11: return eram1[a & 8191];
    case 15:
      if (a >= 65024) {
        if (a == 65280) {
          if (!(io[256]&16)) {
            return ~(16|(Sk[81]*8)|(Sk[82]*4)|(Sk[80]*2)|Sk[79]);
          } else if (!(io[256]&32)) {
            return ~(32|(Sk[40]*8)|(Sk[43]*4)|(Sk[29]*2)|Sk[27]);
          } else {
            return 255;
          }
        }
        return io[a & 511];
      }
    case 12 ... 14: return wram[a & 16383];
  }
}
void w(u16 a, u8 x) { cy+=4;
  switch (a >> 12) {
    case 2 ... 3: rom1 = rom0 + ((x?(x&63):1) << 14); break;
    case 4 ... 5: if (x <= 3) eram1 = eram + (x << 13); break;
    case 8 ... 9: vram[a & 8191] = x; break;
    case 10 ... 11: eram1[a & 8191] = x; break;
    case 15:
      if (a >= 65024) {
        if (a==65350) for(int i=0;i<160;++i) io[i]=r((x<<8)|i);
        io[a & 511] = x;
        break;
      } // fallthrough
    case 12 ... 14: wram[a & 16383] = x; break;
  }
}
void FS(u8 M,int Z,int N,int H,int C) { F=(F&M)|(Z<<7)|(N<<6)|(H<<5)|(C<<4); }
u8 r8() { return r(PC++); }
u16 r16() { u8 l=r8(),h=r8(); return (h<<8)|l; }
u16 pop() { u8 l=r(SP++), h=r(SP++); return (h<<8)|l; }
void push(u16 x) { w(--SP,x>>8);w(--SP,x&255); cy+=4; }

int main(int argc, char** argv) {
  if (argc != 2) return 1;
  rom1 = (rom0 = (u8*)mmap(0,1048576,PROT_READ,MAP_SHARED,open(argv[1],O_RDONLY),0)) + 32768;
  int sav = open("rom.sav",O_CREAT|O_RDWR,0666);
  ftruncate(sav,32768);
  eram1 = eram = (u8*)mmap(0,32768,PROT_READ|PROT_WRITE,MAP_SHARED,sav,0);
  LCDC=145;DIV=44032;
  SDL_Init(SDL_INIT_EVERYTHING);
  SDL_Window* Sw = SDL_CreateWindow("pokegb",100,100,160*5,144*5, SDL_WINDOW_SHOWN);
  SDL_Renderer* Sr = SDL_CreateRenderer(Sw, -1, 0/*SDL_RENDERER_PRESENTVSYNC*/);
  SDL_Texture *St = SDL_CreateTexture(Sr, SDL_PIXELFORMAT_RGBA32,
                                      SDL_TEXTUREACCESS_STREAMING, 160, 144);
  Sk = SDL_GetKeyboardState(NULL);

  while (true) {
    bcy=cy;
#define OP4(x) case x+0:case x+16:case x+32:case x+48
#define OP7(x) case x+0:case x+1:case x+2:case x+3:case x+4:case x+5:case x+7
#define OP7X(x) case x+0:case x+8:case x+16:case x+24:case x+32:case x+40:case x+56
#define OP8X(x) case x+0:case x+8:case x+16:case x+24:case x+32:case x+40:case x+48:case x+56
#define OP56(x) OP7(x):OP7(x+8):OP7(x+16):OP7(x+24):OP7(x+32):OP7(x+40):OP7(x+48):OP7(x+56)
    if (IME&IF&IE) { IF&=~1;halt=IME=0;cy+=8;fc=1;U=64;goto CALLU; }
    else if (!halt) switch ((op = r8())) {
      case 0: break;
      OP4(0x01): *R16[op>>4]=r16(); break;
      case 0x02: case 0x12: w(*R16[op>>4], A); break;
      OP4(0x03): (*R16[op>>4])++; cy+=4; break;
      OP7X(0x04): [](u8&r){++r;FS(16,r==0,0,(r&15)==0,0);}(*R8[op/8]); break;
      OP7X(0x05): [](u8&r){--r;FS(16,r==0,1,(r&15)==15,0);}(*R8[op/8]); break;
      OP7X(0x06): *R8[op/8]=r8(); break;
      case 0x07: fc=(A>>7);A=(A*2)|fc;FS(0,0,0,0,fc); break;
      OP4(0x09): [](u16 x){FS(128,0,0,(HL&4095)+(x&4095)>4095,HL+x>65535);HL+=x;cy+=4;}(*R16[op>>4]); break;
      case 0x0a: case 0x1a: A=r(*R16[op>>4]); break;
      OP4(0x0b): (*R16[op>>4])--; cy+=4; break;
      case 0x0f: fc=A&1;A=(fc<<7)|(A/2);FS(0,0,0,0,fc); break;
      case 0x17: fc=A>>7;A=(A*2)|((F>>4)&1);FS(0,0,0,0,fc); break;
      case 0x20: fc=!(F&128); goto JR;
      case 0x28: fc=F&128; goto JR;
      case 0x30: fc=!(F&16); goto JR;
      case 0x38: fc=F&16; goto JR;
      case 0x18: fc=1; JR: {s8 s=r8(); if(fc) { PC+=s;cy+=4; }} break;
      case 0x22: w(HL++, A); break;
      case 0x27: fc=u=0;
        if ((F&32)||(!(F&64)&&(A&15)>9)) u=6;
        if ((F&16)||(!(F&64)&&A>153)) { u|=96;fc=1; }
        A+=(F&64)?-u:u;
        FS(0x41,A==0,0,0,fc);
        break;
      case 0x2a: A=r(HL++); break;
      case 0x2f: A=~A; FS(129,0,1,1,0); break;
      case 0x32: w(HL--, A); break;
      case 0x34: u=r(HL)+1;FS(16,u==0,0,(u&15)==0,0);w(HL,u); break;
      case 0x35: u=r(HL)-1;FS(16,u==0,1,(u&15)==15,0);w(HL,u); break;
      case 0x36: w(HL,r8()); break;
      case 0x37: FS(128,0,0,0,1); break;
      case 0x3a: A=r(HL--); break;
      case 0x3f: FS(128,0,0,0,!(F&16)); break;
      OP7X(0x46): *R8[(op-0x46)/8]=r(HL); break;
      OP7(0x40): R[1]=*R8[op&7]; break;
      OP7(0x48): R[0]=*R8[op&7]; break;
      OP7(0x50): R[3]=*R8[op&7]; break;
      OP7(0x58): R[2]=*R8[op&7]; break;
      OP7(0x60): R[5]=*R8[op&7]; break;
      OP7(0x68): R[4]=*R8[op&7]; break;
      OP7(0x70): w(HL,*R8[op&7]); break;
      case 0x76: halt=true; break;
      OP7(0x78): A=*R8[op&7]; break;
      OP7(0x80): fc=0;v=*R8[op&7]; goto ADD;
      case 0x86: fc=0;v=r(HL); goto ADD;
      case 0xc6: fc=0;v=r8(); goto ADD;
      OP7(0x88): v=*R8[op&7]; goto ADC;
      case 0x8e: v=r(HL); ADC: fc=(F>>4)&1; ADD: u=A+v+fc;FS(0,u==0,0,(A&15)+(v&15)+fc>15,A+v+fc>255);A=u; break;
      OP7(0x90): fc=0;v=*R8[op&7]; goto SUB;
      case 0x96: fc=0;v=r(HL); goto SUB;
      case 0xd6: fc=0;v=r8(); goto SUB;
      OP7(0x98): v=*R8[op&7];; goto SBC;
      case 0x9e: v=r(HL); goto SBC;
      case 0xde: v=r8(); SBC: fc=(F>>4)&1; SUB: u=A-v-fc;FS(0,u==0,1,(A&15)-(v&15)-fc<0,A-v-fc<0);A=u; break;
      OP7(0xa0): v=*R8[op&7]; goto AND;
      case 0xa6: v=r(HL); goto AND;
      case 0xe6: v=r8(); AND: A&=v; FS(0,A==0,0,1,0); break;
      OP7(0xa8): v=*R8[op&7]; goto XOR;
      case 0xee: v=r8(); XOR: A^=v; FS(0,A==0,0,0,0); break;
      OP7(0xb0): v=*R8[op&7]; goto OR;
      case 0xb6: v=r(HL); goto OR;
      case 0xf6: v=r8(); OR: A|=v; FS(0,A==0,0,0,0); break;
      OP7(0xb8): v=*R8[op&7]; goto CP;
      case 0xbe: v=r(HL); goto CP;
      case 0xfe: v=r8(); CP: FS(0,A==v,1,(A&15)-(v&15)<0,A-v<0); break;
      case 0xc0: fc=!(F&128); goto RET;
      case 0xc8: fc=F&128; goto RET;
      case 0xd0: fc=!(F&16); goto RET;
      case 0xd8: fc=F&16; goto RET;
      case 0xd9: fc=1;IME=31; goto RET;
      case 0xc9: fc=1; RET: cy+=4; if (fc) PC=pop(); break;
      OP4(0xc1): *R162[(op-0xc1)>>4]=pop(); break;
      case 0xc2: fc=!(F&128); goto JP;
      case 0xca: fc=F&128; goto JP;
      case 0xd2: fc=!(F&16); goto JP;
      case 0xda: fc=F&16; goto JP;
      case 0xc3: fc=1; JP: U=r16(); if (fc) {PC=U;cy+=4;} break;
      case 0xc4: fc=!(F&128); goto CALL;
      case 0xcc: fc=F&128; goto CALL;
      case 0xd4: fc=!(F&16); goto CALL;
      case 0xdc: fc=F&16; goto CALL;
      case 0xcd: fc=1; CALL: U=r16(); CALLU: if (fc) {push(PC);PC=U;} break;
      OP4(0xc5): push(*R162[(op-0xc5)>>4]); break;
      case 0xcb: switch((op=r8())) {
        case 0x07: fc=(A>>7)&1;A=(A*2)|fc;FS(0,A==0,0,0,fc); break;
        case 0x0b: fc=E&1;E=(fc<<7)|(E/2);FS(0,E==0,0,0,fc); break;
        case 0x0e: u=r(HL);fc=u&1;u=(fc<<7)|(u/2);FS(0,u==0,0,0,fc);w(HL,u); break;
        OP7(0x10): [](u8&r){fc=r>>7;r=(r*2)|((F>>4)&1);FS(0,r==0,0,0,fc);}(*R8[op&7]); break;
        OP7(0x18): [](u8&r){fc=r&1;r=(r/2)|((F*8)&128);FS(0,r==0,0,0,fc);}(*R8[op&7]); break;
        OP7(0x20): [](u8&r){fc=(r>>7)&1;r*=2; FS(0,r==0,0,0,fc);}(*R8[op&7]); break;
        case 0x2a: fc=D&1;D=(s8)D>>1; FS(0,D==0,0,0,fc); break;
        OP7(0x30): [](u8&r){r=(r*16)|(r>>4); FS(0,r==0,0,0,0);}(*R8[op&7]); break;
        case 0x36: u=r(HL);u=(u*16)|(u>>4); FS(0,u==0,0,0,0);w(HL,u); break;
        OP7(0x38): [](u8&r){fc=r&1;r/=2; FS(0,r==0,0,0,fc);}(*R8[op&7]); break;
        OP8X(0x46): u=r(HL);v=1<<((op-0x46)/8); goto BITU;
        OP56(0x40): v=1<<((op-0x40)/8); u=*R8[op&7]; BITU: FS(16,(u&v)==0,0,1,0); break;
        OP8X(0x86): w(HL,r(HL)&~(1<<((op-0x86)/8))); break;
        OP56(0x80): *R8[op&7]&=~(1<<((op-0x80)/8)); break;
        OP8X(0xc6): w(HL,r(HL)|(1<<((op-0xc6)/8))); break;
        OP56(0xc0): *R8[op&7]|=1<<((op-0xc0)/8); break;
        default: goto BAD;
      } break;
      case 0xe0: v=r8(); goto FF00;
      case 0xe2: v=R[0]; FF00: w(65280|v,A); break;
      case 0xe9: PC=HL; break;
      case 0xea: w(r16(),A); break;
      case 0xf0: A=r(65280|r8()); break;
      case 0xf3: IME=0; break;
      case 0xf8: u=r8();FS(0,0,0,(u8)SP+u>255,SP&15+u&15>15);HL=SP+(s8)u;cy+=4; break;
      case 0xf9: SP=HL;cy+=4; break;
      case 0xfa: A=r(r16()); break;
      case 0xfb: IME=31; break;
 BAD: default:
        fprintf(stderr, "unknown opcode: @%04x: %02x\n", PC, op);
        abort();
        break;
    } else { cy += 4; }
    DIV+=cy-bcy;
    for(;bcy<cy;++bcy) {
      if (LCDC&128) {
        ++dot;
        if (dot== 1 && LY==144) IF|=1;
        if (dot == 456) {
          if (LY < 144) {
            for (int x = 0; x < 160; ++x) {
              u16 base;
              u8 mx, my;
              if ((LCDC&32)&&LY>=WY&&x>=WX-7) {
                base=(LCDC&64)?7168:6144;
                mx=x-WX+7; my=LY-WY;
              } else {
                base=(LCDC&8)?7168:6144;
                mx=x+io[323]; my=LY+io[322];
              }
              u16 tile = vram[base+(my/8)*32+(mx/8)];
              if (!(LCDC&16)) tile=256+(s8)tile;
              u8* d=&vram[(tile*8+(my&7))*2];
              u8 c=(((d[1]>>(7-(mx&7)))&1)*2)|((d[0]>>(7-(mx&7)))&1),p=io[327];
              if (LCDC&2) {
                for (int i = 0; i < 40; ++i) {
                  u8*o=&io[i*4], dx=x-o[1]+8,dy=LY-o[0]+16;
                  assert(!(LCDC&4));
                  if (dx<8&&dy<8) {
                    if (o[3]&64) dy=7-dy;
                    if (o[3]&32) dx=7-dx;
                    d=&vram[(o[2]*8+dy)*2];
                    u8 nc=(((d[1]>>(7-dx))&1)*2)|((d[0]>>(7-dx))&1);
                    if (!((o[3]&128)&&c)&&nc) { c=nc; p=io[o[3]&8?329:328]; break; }
                  }
                }
              }
              fb[LY*160+x]=pal[(p>>(2*c))&3];
            }
          } else if (LY == 144) {
            void* pixels; int pitch;
            SDL_LockTexture(St, NULL, &pixels, &pitch);
            for (int y = 0; y < 144; ++y)
              memcpy((u8 *)pixels + y * pitch, &fb[y * 160], 640);
            SDL_UnlockTexture(St);
            SDL_RenderCopy(Sr, St, NULL, NULL);
            SDL_RenderPresent(Sr);
            SDL_Delay(0);
            SDL_Event e;
            while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) return 0;
          }
          LY = (LY + 1) % 154;
          dot = 0;
        }
      } else {
        LY=dot=0;
      }
    }
  }
}
