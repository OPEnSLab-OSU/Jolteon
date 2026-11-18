#include "Protocol.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static bool space(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n'; }
uint32_t crc32Byte(uint32_t crc,uint8_t byte) {
  crc^=byte;
  for (unsigned i=0;i<8;++i) crc=(crc>>1)^((crc&1) ? 0xedb88320ul : 0);
  return crc;
}
static void skipSpace(const char *&p) { while (space(*p)) ++p; }
bool parseUnsigned(const char *text, uint32_t &value, uint32_t maximum) {
  if (!text || !*text || *text=='-' || *text=='+' || space(*text)) return false;
  char *end; errno=0; unsigned long result=strtoul(text,&end,0);
  if (errno || end==text || *end || result>maximum || result>UINT32_MAX) return false;
  value=uint32_t(result); return true;
}
bool validUdpTarget(const char *ip,const char *port,bool allowUnset) {
  if (!ip || !port || strlen(ip)>15 || strlen(port)>5) return false;
  const char *p=ip; uint32_t octet;
  for (unsigned i=0;i<4;++i) {
    char part[4]; unsigned n=0;
    while (*p>='0' && *p<='9' && n<3) part[n++]=*p++;
    part[n]=0;
    if (!n || (n>1 && part[0]=='0') || !parseUnsigned(part,octet,255)) return false;
    if (i<3 ? *p++!='.' : *p!=0) return false;
  }
  uint32_t value;
  if (!parseUnsigned(port,value,65535)) return false;
  bool unset=!strcmp(ip,"0.0.0.0");
  return unset ? allowUnset && value==0 : value>0;
}
bool responseLine(const char *text, const char *line) {
  if (!text || !line) return false;
  size_t n=strlen(line);
  for (const char *p=text; *p;) {
    while (*p=='\r' || *p=='\n') ++p;
    const char *end=p; while (*end && *end!='\r' && *end!='\n') ++end;
    while (p<end && space(*p)) ++p;
    while (end>p && space(end[-1])) --end;
    if (size_t(end-p)==n && !strncmp(p,line,n)) return true;
    while (*p && *p!='\r' && *p!='\n') ++p;
  }
  return false;
}
bool responsePrefix(const char *text, const char *prefix) {
  if (!text || !prefix) return false;
  size_t n=strlen(prefix);
  for (const char *p=text; *p;) {
    while (*p=='\r' || *p=='\n' || *p==' ' || *p=='\t') ++p;
    if (!strncmp(p,prefix,n)) return true;
    while (*p && *p!='\r' && *p!='\n') ++p;
  }
  return false;
}
bool networkRegistered(const char *reply) {
  if (!responseLine(reply,"OK") || responseLine(reply,"ERROR")) return false;
  for (const char *p=reply; *p;) {
    while (*p=='\r' || *p=='\n' || *p==' ' || *p=='\t') ++p;
    if (!strncmp(p,"+CEREG:",7)) {
      unsigned mode=9,state=9; char end=0;
      int count=sscanf(p,"+CEREG: %1u,%1u%c",&mode,&state,&end);
      if (count>=2 && mode<=5 && (state==1 || state==5) &&
          (count==2 || end==',' || end=='\r' || end=='\n' || end==' ' || end=='\t')) return true;
    }
    while (*p && *p!='\r' && *p!='\n') ++p;
  }
  return false;
}
static bool jsonString(const char *&p) {
  if (*p++!='"') return false;
  while (*p && *p!='"') {
    if (uint8_t(*p)<32) return false;
    if (*p++=='\\') {
      char c=*p++;
      if (c=='u') { for (int i=0;i<4;++i) { if (!isxdigit(uint8_t(*p))) return false; ++p; } }
      else if (!c || !strchr("\"\\/bfnrt",c)) return false;
    }
  }
  if (*p!='"') return false;
  ++p; return true;
}
static bool jsonValue(const char *&p, unsigned depth) {
  if (depth>6) return false;
  skipSpace(p);
  if (*p=='"') return jsonString(p);
  if (*p=='{' || *p=='[') {
    bool object=*p=='{'; char end=object ? '}' : ']'; ++p; skipSpace(p);
    if (*p==end) { ++p; return true; }
    do {
      if (object) { if (!jsonString(p)) return false; skipSpace(p); if (*p++!=':') return false; }
      if (!jsonValue(p,depth+1)) return false;
      skipSpace(p); if (*p==end) { ++p; return true; }
      if (*p++!=',') return false; skipSpace(p);
    } while (*p);
    return false;
  }
  const char *literals[]={"true","false","null"};
  for (const char *literal : literals) {
    size_t n=strlen(literal);
    if (!strncmp(p,literal,n)) { p+=n; return true; }
  }
  if (*p=='-') ++p;
  if (*p=='0') ++p;
  else { if (*p<'1' || *p>'9') return false; do { ++p; } while (isdigit(uint8_t(*p))); }
  if (*p=='.') { ++p; if (!isdigit(uint8_t(*p))) return false; while (isdigit(uint8_t(*p))) ++p; }
  if (*p=='e' || *p=='E') { ++p; if (*p=='+' || *p=='-') ++p; if (!isdigit(uint8_t(*p))) return false; while (isdigit(uint8_t(*p))) ++p; }
  return true;
}
bool jsonFieldEquals(const char *text, const char *key, const char *value) {
  if (!text || !key || !value) return false;
  for (const char *line=text; *line;) {
    const char *p=line; while (*p==' ' || *p=='\t') ++p;
    if (*p=='{') {
      const char *end=p;
      if (jsonValue(end,0)) {
        const char *after=end; while (*after==' ' || *after=='\t') ++after;
        if (!*after || *after=='\r' || *after=='\n') {
          ++p; skipSpace(p); bool found=false, match=false;
          while (p<end && *p!='}') {
            const char *name=p+1; if (!jsonString(p)) break;
            bool wanted=size_t(p-name-1)==strlen(key) && !strncmp(name,key,p-name-1);
            skipSpace(p); ++p; skipSpace(p); const char *begin=p;
            if (!jsonValue(p,1)) break;
            if (wanted) {
              if (found) return false; // Duplicate keys are ambiguous.
              found=true; match=size_t(p-begin)==strlen(value) && !strncmp(begin,value,p-begin);
            }
            skipSpace(p); if (*p==',') { ++p; skipSpace(p); }
          }
          if (found && match) return true;
        }
      }
    }
    while (*line && *line!='\n' && *line!='\r') ++line;
    while (*line=='\n' || *line=='\r') ++line;
  }
  return false;
}
static bool token(const char *line, const char *key, char *out, size_t capacity) {
  size_t n=strlen(key); bool found=false;
  for (const char *p=line; *p;) {
    while (*p==' ') ++p;
    const char *end=p; while (*end && *end!=' ') ++end;
    if (size_t(end-p)>n && !strncmp(p,key,n) && p[n]=='=') {
      size_t len=end-p-n-1;
      if (found || len>=capacity) return false;
      memcpy(out,p+n+1,len); out[len]=0; found=true;
    }
    p=end;
  }
  return found;
}
bool bridgeAck(const char *line, unsigned payloadLength, const char *tag) {
  if (!line || !tag || strncmp(line,"ok tx both ",11)) return false;
  char field[32]; uint32_t number;
  if (!token(line,"len",field,sizeof(field)) || !parseUnsigned(field,number,244) || number!=payloadLength) return false;
  if (!token(line,"accepted",field,sizeof(field)) || !parseUnsigned(field,number,2) || !number) return false;
  if (!token(line,"uart",field,sizeof(field)) || strcmp(field,"source")) return false;
  return token(line,"tag",field,sizeof(field)) && !strcmp(field,tag);
}
static bool bcdInRange(uint8_t b, unsigned lo, unsigned hi) {
  unsigned value=(b>>4)*10+(b&15);
  return (b&15)<10 && (b>>4)<10 && value>=lo && value<=hi;
}
bool rtcCalendarValid(const uint8_t t[7]) {
  if (!t || !bcdInRange(t[0],0,59) || !bcdInRange(t[1],0,59) || t[2]&0x80 || t[3]<1 || t[3]>7) return false;
  if (t[2]&0x40) { if (!bcdInRange(t[2]&0x1f,1,12)) return false; }
  else if (!bcdInRange(t[2],0,23)) return false;
  if (t[5]&0x60 || !bcdInRange(t[5]&0x1f,1,12) || !bcdInRange(t[6],0,99)) return false;
  unsigned month=((t[5]&0x10)>>4)*10+(t[5]&15), year=2000+(t[5]&0x80 ? 100 : 0)+(t[6]>>4)*10+(t[6]&15);
  static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};
  unsigned maxDay=days[month-1]+(month==2 && year%4==0 && (year%100!=0 || year%400==0));
  return bcdInRange(t[4],1,maxDay);
}
