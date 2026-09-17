#pragma once
// Small bounded JSON reader for local character manifests.
#include <map>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdlib>
#include <cctype>
namespace character_json {
struct Value {
  std::string text;
  std::map<std::string, Value> object;
  std::vector<Value> array;
  char kind = 0;
  const Value& at(const char* key) const { if(kind!='{')throw std::runtime_error("Expected object");return object.at(key); }
  const std::string& string() const { if(kind!='s')throw std::runtime_error("Expected string");return text; }
  const std::vector<Value>& items() const { if(kind!='[')throw std::runtime_error("Expected array");return array; }
  int number() const {
    if (kind != 'n') throw std::runtime_error("Expected integer");
    size_t end; int n = std::stoi(text, &end);
    if (end != text.size()) throw std::runtime_error("Expected integer");
    return n;
  }
};
class Reader {
  const std::string& s; size_t p = 0;
  void ws(){while(p<s.size() && std::isspace((unsigned char)s[p])) ++p;}
  char take(){if(p==s.size())throw std::runtime_error("Truncated JSON");return s[p++];}
  std::string str(){
    if(take()!='"')throw std::runtime_error("Expected string");
    std::string out;
    while(true){char c=take();if(c=='"')return out;if((unsigned char)c<32)throw std::runtime_error("Control in string");
      if(c=='\\'){c=take();switch(c){case 'n':c='\n';break;case 'r':c='\r';break;case 't':c='\t';break;case '"':case '\\':case '/':break;default:throw std::runtime_error("Unsupported JSON escape");}}
      out+=c;
    }
  }
  Value value(int depth){
    if(depth>16)throw std::runtime_error("JSON nesting too deep");ws();Value v;
    if(p==s.size())throw std::runtime_error("Missing value");
    char c=s[p];
    if(c=='"'){v.kind='s';v.text=str();return v;}
    if(c=='{'||c=='['){v.kind=take();ws();char end=c=='{'?'}':']';if(p<s.size()&&s[p]==end){++p;return v;}
      while(true){if(c=='{'){ws();auto key=str();ws();if(take()!=':')throw std::runtime_error("Missing colon");if(!v.object.emplace(key,value(depth+1)).second)throw std::runtime_error("Duplicate key");}
        else v.array.push_back(value(depth+1));ws();char sep=take();if(sep==end)return v;if(sep!=',')throw std::runtime_error("Missing comma");}
    }
    v.kind='n';size_t start=p;if(s[p]=='-')++p;while(p<s.size()&&std::isdigit((unsigned char)s[p]))++p;
    if(p==start||(p==start+1&&s[start]=='-'))throw std::runtime_error("Expected value");v.text=s.substr(start,p-start);return v;
  }
public:
  explicit Reader(const std::string& input):s(input){}
  Value read(){auto v=value(0);ws();if(p!=s.size())throw std::runtime_error("Trailing JSON data");return v;}
};
inline std::string quote(const std::string&s){std::string out="\"";for(char c:s){if(c=='"'||c=='\\')out+='\\';if((unsigned char)c>=32)out+=c;}return out+'"';}
}
