#include "inde/persistence/json.hpp"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace inde::persistence::json {

Error::Error(std::string message, std::size_t line, std::size_t column)
    : std::runtime_error(std::move(message) + " (linha " + std::to_string(line) +
        ", coluna " + std::to_string(column) + ")"), line_(line), column_(column) {}

bool Value::is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(data_); }
const std::string& Value::as_string() const {
    if (const auto* value=std::get_if<std::string>(&data_)) return *value;
    throw std::runtime_error("O valor JSON não é uma string");
}
std::int64_t Value::as_integer() const {
    if (const auto* value=std::get_if<std::int64_t>(&data_)) return *value;
    throw std::runtime_error("O valor JSON não é um inteiro");
}
bool Value::as_boolean() const {
    if (const auto* value=std::get_if<bool>(&data_)) return *value;
    throw std::runtime_error("O valor JSON não é booleano");
}
const Value::Array& Value::as_array() const {
    if (const auto* value=std::get_if<Array>(&data_)) return *value;
    throw std::runtime_error("O valor JSON não é uma lista");
}
const Value::Object& Value::as_object() const {
    if (const auto* value=std::get_if<Object>(&data_)) return *value;
    throw std::runtime_error("O valor JSON não é um objeto");
}
const Value& Value::at(std::string_view key) const {
    const auto& object=as_object(); const auto found=object.find(key);
    if(found==object.end())throw std::runtime_error("Campo JSON obrigatório ausente: "+std::string(key));
    return found->second;
}

namespace {
class Parser {
public:
    explicit Parser(std::string_view source):source_(source){}
    Value run(){skip();auto value=parse_value();skip();if(!eof())fail("Conteúdo após o fim do JSON");return value;}
private:
    std::string_view source_;std::size_t index_{0},line_{1},column_{1};
    bool eof()const{return index_>=source_.size();}
    char peek()const{return eof()?'\0':source_[index_];}
    char take(){if(eof())fail("Fim inesperado do JSON");char c=source_[index_++];if(c=='\n'){++line_;column_=1;}else ++column_;return c;}
    [[noreturn]] void fail(const std::string& message)const{throw Error(message,line_,column_);}
    void skip(){while(!eof()&&(peek()==' '||peek()=='\t'||peek()=='\r'||peek()=='\n'))take();}
    bool consume(std::string_view text){if(source_.substr(index_,text.size())!=text)return false;for(std::size_t i=0;i<text.size();++i)take();return true;}
    Value parse_value(){skip();switch(peek()){
        case '{':return parse_object();case '[':return parse_array();case '"':return parse_string();
        case 't':if(consume("true"))return true;break;case 'f':if(consume("false"))return false;break;
        case 'n':if(consume("null"))return nullptr;break;default:if(peek()=='-'||(peek()>='0'&&peek()<='9'))return parse_number();}
        fail("Valor JSON inválido");}
    Value parse_object(){take();Value::Object object;skip();if(peek()=='}'){take();return object;}while(true){
        skip();if(peek()!='"')fail("A chave do objeto deve ser uma string");const auto key=parse_string().as_string();
        if (object.contains(key)) fail("Chave JSON duplicada: " + key);
        skip();
        if (take() != ':') fail("Esperado ':' após a chave");
        object.emplace(key,parse_value());skip();const char separator=take();if(separator=='}')break;if(separator!=',')fail("Esperado ',' ou '}'");}
        return object;}
    Value parse_array(){take();Value::Array array;skip();if(peek()==']'){take();return array;}while(true){
        array.push_back(parse_value());skip();const char separator=take();if(separator==']')break;if(separator!=',')fail("Esperado ',' ou ']'");}return array;}
    static void append_utf8(std::string& out,std::uint32_t cp){
        if(cp<=0x7f)out+=static_cast<char>(cp);else if(cp<=0x7ff){out+=static_cast<char>(0xc0|(cp>>6));out+=static_cast<char>(0x80|(cp&0x3f));}
        else if(cp<=0xffff){out+=static_cast<char>(0xe0|(cp>>12));out+=static_cast<char>(0x80|((cp>>6)&0x3f));out+=static_cast<char>(0x80|(cp&0x3f));}
        else{out+=static_cast<char>(0xf0|(cp>>18));out+=static_cast<char>(0x80|((cp>>12)&0x3f));out+=static_cast<char>(0x80|((cp>>6)&0x3f));out+=static_cast<char>(0x80|(cp&0x3f));}}
    std::uint32_t hex4(){std::uint32_t value=0;for(int i=0;i<4;++i){const char c=take();value<<=4;if(c>='0'&&c<='9')value+=c-'0';else if(c>='a'&&c<='f')value+=10+c-'a';else if(c>='A'&&c<='F')value+=10+c-'A';else fail("Escape Unicode inválido");}return value;}
    Value parse_string(){take();std::string out;while(true){if(eof())fail("String JSON não terminada");const unsigned char c=take();
        if (c == '"') break;
        if (c < 0x20) fail("Caractere de controle em string JSON");
        if (c != '\\') { out += static_cast<char>(c); continue; }
        const char escaped=take();switch(escaped){case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;
        case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
        case 'u':{auto cp=hex4();if(cp>=0xd800&&cp<=0xdbff){if(take()!='\\'||take()!='u')fail("Par substituto Unicode incompleto");const auto low=hex4();if(low<0xdc00||low>0xdfff)fail("Par substituto Unicode inválido");cp=0x10000+((cp-0xd800)<<10)+(low-0xdc00);}else if(cp>=0xdc00&&cp<=0xdfff)fail("Substituto Unicode isolado");append_utf8(out,cp);break;}
        default:fail("Escape de string JSON inválido");}}return out;}
    Value parse_number(){const auto start=index_;if(peek()=='-')take();if(peek()=='0')take();else{if(peek()<'1'||peek()>'9')fail("Número JSON inválido");while(peek()>='0'&&peek()<='9')take();}
        bool decimal=false;if(peek()=='.'){decimal=true;take();if(peek()<'0'||peek()>'9')fail("Fração JSON inválida");while(peek()>='0'&&peek()<='9')take();}
        if(peek()=='e'||peek()=='E'){decimal=true;take();if(peek()=='+'||peek()=='-')take();if(peek()<'0'||peek()>'9')fail("Expoente JSON inválido");while(peek()>='0'&&peek()<='9')take();}
        const auto token=source_.substr(start,index_-start);if(decimal){try{const double value=std::stod(std::string(token));if(!std::isfinite(value))fail("Número JSON fora do limite");return value;}catch(...){fail("Número JSON inválido");}}
        std::int64_t value{};const auto result=std::from_chars(token.data(),token.data()+token.size(),value);if(result.ec!=std::errc{})fail("Inteiro JSON fora do limite");return value;}
};

std::string quote(const std::string& value){std::ostringstream out;out<<'"';for(const unsigned char c:value)switch(c){case '"':out<<"\\\"";break;case '\\':out<<"\\\\";break;case '\b':out<<"\\b";break;case '\f':out<<"\\f";break;case '\n':out<<"\\n";break;case '\r':out<<"\\r";break;case '\t':out<<"\\t";break;default:if(c<0x20)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<int>(c)<<std::dec;else out<<static_cast<char>(c);}out<<'"';return out.str();}
void write(const Value& value,std::ostringstream& out,bool pretty,int depth){
    const auto indent=[&](int level){if(pretty)out<<std::string(level*2,' ');};
    std::visit([&](const auto& item){using T=std::decay_t<decltype(item)>;
        if constexpr(std::is_same_v<T,std::nullptr_t>)out<<"null";else if constexpr(std::is_same_v<T,bool>)out<<(item?"true":"false");
        else if constexpr(std::is_same_v<T,std::int64_t>||std::is_same_v<T,double>)out<<item;else if constexpr(std::is_same_v<T,std::string>)out<<quote(item);
        else if constexpr(std::is_same_v<T,Value::Array>){out<<'[';for(std::size_t i=0;i<item.size();++i){if(i)out<<',';if(pretty){out<<'\n';indent(depth+1);}write(item[i],out,pretty,depth+1);}if(pretty&&!item.empty()){out<<'\n';indent(depth);}out<<']';}
        else if constexpr(std::is_same_v<T,Value::Object>){out<<'{';std::size_t i=0;for(const auto&[key,child]:item){if(i++)out<<',';if(pretty){out<<'\n';indent(depth+1);}out<<quote(key)<< (pretty?": ":":");write(child,out,pretty,depth+1);}if(pretty&&!item.empty()){out<<'\n';indent(depth);}out<<'}';}
    },value.storage());}
}

Value parse(std::string_view source){return Parser(source).run();}
std::string serialize(const Value& value,bool pretty){std::ostringstream out;write(value,out,pretty,0);out<<'\n';return out.str();}
} // namespace inde::persistence::json
