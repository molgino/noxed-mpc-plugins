// TB-303-style two-pass pattern writing. Pitch notes and timing cells are separate.
// Audio-thread command consumption uses fixed-size data only; UI refresh happens elsewhere.
#pragma once
class AcidPatternEditor {
public:
  enum { Mode=84, Cursor, Octave, Style, PitchCount, TimeFirst,
         KeyFirst=TimeFirst+16, Back=KeyFirst+13, Tap, Note, Tie, Rest, Clear, Accent, Slide,
         Display, Count };
  void changed(int p) {
    if(p==Mode) pending_.fetch_or(uint32_t(1)<<31);
    else if(p>=KeyFirst && p<=Slide) pending_.fetch_or(uint32_t(1)<<(p-KeyFirst));
  }
  void reset(){pending_.store(0);pitchStart_=timeStart_=checkStart_=false;audition=-1;}
  bool consume(mpc::Plugin &p) {
    const uint32_t commands=pending_.exchange(0);
    if(!commands)return false;
    if((commands>>31) && p.getInt(Mode)!=0){
      import(p);p.set(Cursor,1);pitchStart_=timeStart_=checkStart_=true;
    }
    for(int action=KeyFirst;action<=Slide;++action)if(commands&(uint32_t(1)<<(action-KeyFirst))){
      int mode=p.getInt(Mode),cursor=p.getInt(Cursor)-1;
      if(action==Clear){
        p.set(Style,1);p.set(PitchCount,1);p.set(17,16);p.set(Cursor,1);
        for(int i=0;i<16;++i){p.set(19+i*4,36);p.set(21+i*4,0);p.set(22+i*4,0);p.set(TimeFirst+i,2);}
        pitchStart_=timeStart_=checkStart_=true;
      }else if(action==Back || action==Tap){
        const int count=mode==2?p.getInt(17):p.getInt(PitchCount);
        if(!(mode==1 && action==Tap && checkStart_))cursor=(cursor+(action==Back?-1:1)+count)%count;
        checkStart_=false;p.set(Cursor,cursor+1);
        if(mode==1)audition=p.getInt(19+cursor*4);
      }else if(action>=KeyFirst && action<KeyFirst+13){
        int key=36+(action-KeyFirst)+12*p.getInt(Octave);
        audition=key;
        if(mode==1){
          import(p);p.set(19+cursor*4,key);
          p.set(PitchCount,pitchStart_?cursor+1:std::max(p.getInt(PitchCount),cursor+1));
          pitchStart_=checkStart_=false;p.set(Cursor,(cursor+1)%16+1);
        }
      }else if(mode==1 && (action==Accent || action==Slide)){
        import(p);int target=19+cursor*4+(action==Accent?2:3);p.set(target,1-p.getInt(target));
      }else if(mode==2 && action>=Note && action<=Rest){
        import(p);p.set(TimeFirst+cursor,action-Note);
        p.set(17,timeStart_?cursor+1:std::max(p.getInt(17),cursor+1));timeStart_=false;
        p.set(Cursor,(cursor+1)%16+1);
      }
    }
    p.set(Display,1-p.getInt(Display));
    return true;
  }
  int audition=-1;
private:
  void import(mpc::Plugin &p){
    if(p.getBool(Style))return;
    int count=0;
    for(int i=0;i<16;++i){
      bool play=p.getBool(20+i*4);p.set(TimeFirst+i,play?0:2);
      if(play){
        int src=19+i*4,dst=19+count*4;
        float note=p.get(src),accent=p.get(src+2),slide=p.get(src+3);
        p.set(dst,note);p.set(dst+2,accent);p.set(dst+3,slide);++count;
      }
    }
    p.set(PitchCount,std::max(1,count));p.set(Style,1);
  }
  std::atomic<uint32_t> pending_{0};
  bool pitchStart_=false,timeStart_=false,checkStart_=false;
};
