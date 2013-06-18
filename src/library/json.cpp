#include "json.hpp"
#include "string.hpp"
#include <limits>
#include <climits>
#include <sstream>

namespace JSON
{
const char* error_desc[] = {
	"Success",
	"Not a number",
	"Not a string",
	"Not a boolean",
	"Not an array",
	"Not an object",
	"Not array nor object",
	"Invalid index",
	"Invalid key",
	"Invalid instance",
	"Trailing pointer escape character",
	"Invalid pointer escape",
	"Bad hexadecimal character",
	"Invalid surrogate escape",
	"Invalid escape sequence",
	"Truncated string",
	"Unknown value type",
	"Garbage after end of JSON",
	"Truncated JSON",
	"Unexpected comma",
	"Unexpected colon",
	"Unexpected right brace",
	"Unexpected right braket",
	"Invalid number syntax",
	"Expected string as object key",
	"Expected colon",
	"Expected comma",
	"Unknown token type",
	"Bad pointer append",
	"Bad pointer index",
	"Iterator past the end",
	"Iterator points to deleted object",
	"Iterator points to wrong object",
	"Illegal character escaped",
	"Control character in string",
	"Invalid value subtype",
	"Bad JSON patch",
	"JSON patch test failed",
	"JSON patch illegal move",
};

node number_tag::operator()(double v) const { return node(*this, v); }
node number_tag::operator()(uint64_t v) const { return node(*this, v); }
node number_tag::operator()(int64_t v) const { return node(*this, v); }
node string_tag::operator()(const std::string& s) const { return node(*this, s); }
node string_tag::operator()(const std::u32string& s) const { return node(*this, s); }
node boolean_tag::operator()(bool v)  { return node(*this, v); }
node array_tag::operator()() const { return node(*this); }
node object_tag::operator()() const { return node(*this); }
node null_tag::operator()() const { return node(*this); }

node i(int64_t n) { return number(n); }
node u(uint64_t n) { return number(n); }
node f(double n) { return number(n); }
node b(bool bl) { return boolean(bl); }
node s(const std::string& st) { return string(st); }
node s(const std::u32string& st) { return string(st); }
node n() { return null(); }

number_tag number;
string_tag string;
boolean_tag boolean;
array_tag array;
object_tag object;
null_tag null;

template<> void node::number_holder::from(double val) { sub = 0; n.n0 = val; }
template<> void node::number_holder::from(uint64_t val) { sub = 1; n.n1 = val; }
template<> void node::number_holder::from(int64_t val) { sub = 2; n.n2 = val; }

namespace
{
	unsigned numchar(char c)
	{
		switch(c) {
		case '0': return 0;
		case '1': case '2': case '3': case '4': case '5':
		case '6': case '7': case '8': case '9': return 1;
		case '.': return 2;
		case 'e': return 3;
		case '+': return 4;
		case '-': return 5;
		default: return 6;
		}
	}

	uint32_t numdfa[] = {
		0x0F1FFF32,	//0: STATE_INITIAL
		0x0FFFFF32,	//1: STATE_AFTER_MINUS
		0x1FFF64FF,	//2: STATE_INT_Z
		0x1FFF6433,	//3: STATE_INT_NZ
		0x0FFFFF55,	//4: STATE_DECIMAL
		0x1FFF6F55,	//5: STATE_DECIMAL_N
		0x0F77FF88,	//6: STATE_EXP
		0x0FFFFF88,	//7: STATE_EXP_SIGN
		0x1FFFFF88,	//8: STATE_EXP_N
	};
}

node::number_holder::number_holder(const std::string& expr, size_t& ptr, size_t len)
{
	//-?(0|1-9[0-9]+)(.[0-9]+)?([eE][+-]?[0-9]+)?
	int state = 0;
	size_t tmp = ptr;
	std::string x;
	while(tmp < len) {
		unsigned c = numchar(expr[tmp]);
		unsigned ns = (numdfa[state] >> (4 * c)) & 0xF;
		if(ns == 0xF)
			break;
		else
			state = ns;
		tmp++;
	}
	if(!(numdfa[state] >> 28))
		goto bad;
	x = expr.substr(ptr, tmp - ptr);
	ptr = tmp;
	try {
		n.n1 = parse_value<uint64_t>(x);
		sub = 1;
		return;
	} catch(...) {}
	try {
		n.n2 = parse_value<int64_t>(x);
		sub = 2;
		return;
	} catch(...) {}
	try {
		n.n0 = parse_value<double>(x);
		sub = 0;
		return;
	} catch(...) {}
bad:
	throw error(ERR_INVALID_NUMBER);
}


void node::number_holder::write(std::ostream& s) const
{
	switch(sub) {
	case 0: s << n.n0; break;
	case 1: s << n.n1; break;
	case 2: s << n.n2; break;
	}
}

template<typename T> bool node::number_holder::cmp(const T& num) const
{
	switch(sub) {
	case 0: return n.n0 == num;
	case 1: return n.n1 == num;
	case 2: return n.n2 == num;
	}
	throw error(ERR_UNKNOWN_SUBTYPE);
}

template<> bool node::number_holder::cmp(const int64_t& num) const
{
	switch(sub) {
	case 0: return n.n0 == num;
	case 1: return num >= 0 && n.n1 == static_cast<uint64_t>(num);
	case 2: return n.n2 == num;
	}
	throw error(ERR_UNKNOWN_SUBTYPE);
}

template<> bool node::number_holder::cmp(const uint64_t& num) const
{
	switch(sub) {
	case 0: return n.n0 == num;
	case 1: return n.n1 == num;
	case 2: return n.n2 >= 0 && static_cast<uint64_t>(n.n2) == num;
	}
	throw error(ERR_UNKNOWN_SUBTYPE);
}


bool node::number_holder::operator==(const number_holder& h) const
{
	switch(sub) {
	case 0:	return h.cmp(n.n0);
	case 1:	return h.cmp(n.n1);
	case 2:	return h.cmp(n.n2);
	}
	throw error(ERR_UNKNOWN_SUBTYPE);
}

node::node() throw() : node(null) {}
node::node(null_tag) throw() { vtype = null; }
node::node(boolean_tag, bool b) throw() { vtype = boolean; _boolean = b; }
node::node(string_tag, const std::u32string& str) throw(std::bad_alloc) { vtype = string; _string = str; }
node::node(string_tag, const std::string& str) throw(std::bad_alloc) { vtype = string; _string = to_u32string(str); }
node::node(number_tag, double n) throw() { vtype = number; _number.from<double>(n); }
node::node(number_tag, int64_t n) throw() { vtype = number; _number.from<int64_t>(n); }
node::node(number_tag, uint64_t n) throw() { vtype = number; _number.from<uint64_t>(n); }
node::node(array_tag) throw() { vtype = array; }
node::node(object_tag) throw() { vtype = object; }
int node::type() const throw() { return vtype; }

node& node::set(null_tag) throw() { set_helper<uint64_t>(0); vtype = null; return *this; }
node& node::set(boolean_tag, bool n) throw() { set_helper<uint64_t>(0); vtype = boolean; _boolean = n; return *this; }
node& node::set(number_tag, double n) throw() { set_helper<double>(n); return *this; }
node& node::set(number_tag, int64_t n) throw() { set_helper<int64_t>(n); return *this; }
node& node::set(number_tag, uint64_t n) throw() { set_helper<int64_t>(n); return *this; }

node& node::set(string_tag, const std::u32string& key) throw(std::bad_alloc)
{
	std::u32string tmp = key;
	std::swap(_string, tmp);
	vtype = string;
	xarray.clear();
	xobject.clear();
	return *this;
}

double node::as_double() const throw(error)
{
	return get_number_helper<double>();
}

int64_t node::as_int() const throw(error)
{
	return get_number_helper<int64_t>();
}

uint64_t node::as_uint() const throw(error)
{
	return get_number_helper<uint64_t>();
}

const std::u32string& node::as_string() const throw(std::bad_alloc, error)
{
	if(vtype != string)
		throw error(ERR_NOT_A_STRING);
	return _string;
}

bool node::as_bool() const throw(error)
{
	if(vtype != boolean)
		throw error(ERR_NOT_A_BOOLEAN);
	return _boolean;
}

size_t node::index_count() const throw(error)
{
	if(vtype != array)
		throw error(ERR_NOT_AN_ARRAY);
	return xarray.size();
}

const node& node::index(size_t index) const throw(error)
{
	if(vtype != array)
		throw error(ERR_NOT_AN_ARRAY);
	if(index >= xarray_index.size())
		throw error(ERR_INDEX_INVALID);
	return *xarray_index[index];
}

node& node::index(size_t index) throw(error)
{
	if(vtype != array)
		throw error(ERR_NOT_AN_ARRAY);
	if(index >= xarray_index.size())
		throw error(ERR_INDEX_INVALID);
	return *xarray_index[index];
}

size_t node::field_count(const std::u32string& key) const throw(error)
{
	if(vtype != object)
		throw error(ERR_NOT_AN_OBJECT);
	if(!xobject.count(key))
		return 0;
	return xobject.find(key)->second.size();
}

bool node::field_exists(const std::u32string& key) const throw(error)
{
	return (field_count(key) > 0);
}

const node& node::field(const std::u32string& key, size_t subindex) const throw(error)
{
	if(vtype != object)
		throw error(ERR_NOT_AN_OBJECT);
	if(!xobject.count(key))
		throw error(ERR_KEY_INVALID);
	const std::list<node>& l = xobject.find(key)->second;
	size_t j = 0;
	for(auto i = l.begin(); i != l.end(); i++, j++) {
		if(j == subindex)
			return *i;;
	}
	throw error(ERR_INSTANCE_INVALID);
}

node& node::field(const std::u32string& key, size_t subindex) throw(error)
{
	if(vtype != object)
		throw error(ERR_NOT_AN_OBJECT);
	if(!xobject.count(key))
		throw error(ERR_KEY_INVALID);
	std::list<node>& l = xobject.find(key)->second;
	size_t j = 0;
	for(auto i = l.begin(); i != l.end(); i++, j++) {
		if(j == subindex)
			return *i;;
	}
	throw error(ERR_INSTANCE_INVALID);
}

node::node(const node& _node) throw(std::bad_alloc)
{
	std::u32string tmp1 = _node._string;
	std::list<node> tmp2 = _node.xarray;
	std::map<std::u32string, std::list<node>> tmp3 = _node.xobject;
	std::vector<node*> tmp4 = _node.xarray_index;

	vtype = _node.vtype;
	_number = _node._number;
	_boolean = _node._boolean;
	_string = _node._string;
	xarray = _node.xarray;
	xobject = _node.xobject;
	xarray_index = _node.xarray_index;
	fixup_nodes(_node);
}

node& node::operator=(const node& _node) throw(std::bad_alloc)
{
	if(this == &_node)
		return *this;
	std::u32string tmp1 = _node._string;
	std::list<node> tmp2 = _node.xarray;
	std::map<std::u32string, std::list<node>> tmp3 = _node.xobject;
	std::vector<node*> tmp4 = _node.xarray_index;

	vtype = _node.vtype;
	_number = _node._number;
	_boolean = _node._boolean;
	std::swap(_string, tmp1);
	std::swap(xarray, tmp2);
	std::swap(xobject, tmp3);
	std::swap(xarray_index, tmp4);
	fixup_nodes(_node);
	return *this;
}

node& node::append(const node& _node) throw(std::bad_alloc, error)
{
	if(vtype != array)
		throw error(ERR_NOT_AN_ARRAY);
	bool p = false;
	try {
		xarray.push_back(_node);
		p = true;
		node* ptr = &*xarray.rbegin();
		xarray_index.push_back(ptr);
		return *ptr;
	} catch(std::bad_alloc& e) {
		if(p)
			xarray.pop_back();
		throw;
	}
}

node& node::insert(const std::u32string& key, const node& _node) throw(std::bad_alloc, error)
{
	if(vtype != object)
		throw error(ERR_NOT_AN_OBJECT);
	try {
		xobject[key].push_back(_node);
		return *xobject[key].rbegin();
	} catch(...) {
		if(xobject.count(key) && xobject[key].empty())
			xobject.erase(key);
		throw;
	}
}

namespace
{
	std::u32string jsonptr_unescape(const std::u32string& c, size_t start, size_t end)
	{
		std::basic_ostringstream<char32_t> out;
		for(size_t ptr = start; ptr < end; ptr++) {
			if(c[ptr] == '~') {
				if(ptr == end - 1)
					throw error(ERR_POINTER_TRAILING_ESCAPE);
				ptr++;
				if(c[ptr] == '0')
					out << U"~";
				else if(c[ptr] == '1')
					out << U"/";
				else
					throw error(ERR_POINTER_INVALID_ESCAPE);
			} else
				out << c[ptr];
		}
		return out.str();
	}
}

const node& node::follow(const std::u32string& pointer) const throw(std::bad_alloc, error)
{
	const node* current = this;
	size_t ptr = 0;
	while(ptr < pointer.length()) {
		size_t p = pointer.find_first_of(U"/", ptr);
		if(p > pointer.length())
			p = pointer.length();
		std::u32string c = jsonptr_unescape(pointer, ptr, p);
		if(current->vtype == array) {
			if(c == U"-")
				throw error(ERR_POINTER_BAD_APPEND);
			size_t idx;
			try {
				idx = parse_value<size_t>(to_u8string(c));
			} catch(std::bad_alloc& e) {
				throw;
			} catch(...) {
				throw error(ERR_POINTER_BAD_INDEX);
			}
			current = &current->index(idx);
		} else if(current->vtype == object) {
			current = &current->field(c);
		} else
			throw error(ERR_NOT_ARRAY_NOR_OBJECT);
		ptr = p + 1;
	}
	return *current;
}

node& node::follow(const std::u32string& pointer) throw(std::bad_alloc, error)
{
	node* current = this;
	size_t ptr = 0;
	while(ptr < pointer.length()) {
		size_t p = pointer.find_first_of(U"/", ptr);
		if(p > pointer.length())
			p = pointer.length();
		std::u32string c = jsonptr_unescape(pointer, ptr, p);
		if(current->vtype == array) {
			if(c == U"-")
				throw error(ERR_POINTER_BAD_APPEND);
			size_t idx;
			try {
				idx = parse_value<size_t>(to_u8string(c));
			} catch(std::bad_alloc& e) {
				throw;
			} catch(...) {
				throw error(ERR_POINTER_BAD_INDEX);
			}
			current = &current->index(idx);
		} else if(current->vtype == object) {
			current = &current->field(c);
		} else
			throw error(ERR_NOT_ARRAY_NOR_OBJECT);
		ptr = p + 1;
	}
	return *current;
}

namespace
{
	struct json_token
	{
		enum ttype { TSTRING, TNUMBER, TOBJECT, TARRAY, TINVALID, TCOMMA, TOBJECT_END, TARRAY_END, TCOLON,
			TEOF, TTRUE, TFALSE, TNULL };
		ttype type;
		std::u32string value;
		json_token(enum ttype t, const std::u32string& v) { type = t; value = v; }
		json_token(enum ttype t) { type = t; }
	};

	uint32_t parse_hex(int32_t ch)
	{
		switch(ch) {
		case '0': return 0;
		case '1': return 1;
		case '2': return 2;
		case '3': return 3;
		case '4': return 4;
		case '5': return 5;
		case '6': return 6;
		case '7': return 7;
		case '8': return 8;
		case '9': return 9;
		case 'A': case 'a': return 10;
		case 'B': case 'b': return 11;
		case 'C': case 'c': return 12;
		case 'D': case 'd': return 13;
		case 'E': case 'e': return 14;
		case 'F': case 'f': return 15;
		default: throw error(ERR_BAD_HEX);
		}
	}

	class iterator_counter
	{
	public:
		iterator_counter(size_t& _count) : count(_count) { count = 0; }
		char32_t& operator*() { count++; return x; }
		iterator_counter& operator++() { return *this; }
		size_t get_count() { return count; }
	private:
		char32_t x;
		size_t& count;
	};

	//STATE_NORMAL 			0
	//STATE_ESCAPE 			1
	//STATE_ESCAPE_HEX0		2
	//STATE_ESCAPE_HEX1		3
	//STATE_ESCAPE_HEX2		4
	//STATE_ESCAPE_HEX3		5
	//STATE_ESCAPE_SURROGATE	6
	//STATE_ESCAPE_SURROGATE2	7
	//STATE_ESCAPE_SURROGATE_HEX0	8
	//STATE_ESCAPE_SURROGATE_HEX1	9
	//STATE_ESCAPE_SURROGATE_HEX2	10
	//STATE_ESCAPE_SURROGATE_HEX3	11

	template<typename T> size_t read_string_impl(T target, const std::string& doc, size_t ptr, size_t len)
	{
		uint16_t ustate = utf8_initial_state;
		int estate = 0;
		uint32_t extra = 0;
		uint32_t tmp;
		size_t i;
		for(i = ptr; i <= len; i++) {
			int ch = -1;
			if(i < len)
				ch = (unsigned char)doc[i];
			int32_t uch = utf8_parse_byte(ch, ustate);
			if(uch < 0)
				continue;
			//Okay, have Unicode codepoint decoded.
			switch(estate) {
			case 0:
				extra = 0;
				if(uch < 32)
					throw error(ERR_CONTROL_CHARACTER);
				if(uch == '\"')
					goto out;
				if(uch == '\\')
					estate = 1;
				else {
					*target = uch;
					++target;
				}
				break;
			case 1:
				switch(uch) {
				case '\"': *target = U'\"'; ++target; estate = 0; break;
				case '\\': *target = U'\\'; ++target; estate = 0; break;
				case '/': *target = U'/'; ++target; estate = 0; break;
				case 'b': *target = U'\b'; ++target; estate = 0; break;
				case 'f': *target = U'\f'; ++target; estate = 0; break;
				case 'n': *target = U'\n'; ++target; estate = 0; break;
				case 'r': *target = U'\r'; ++target; estate = 0; break;
				case 't': *target = U'\t'; ++target; estate = 0; break;
				case 'u':
					estate = 2;
					break;
				default:
					throw error(ERR_INVALID_ESCAPE);
				}
				break;
			case 2: case 3: case 4: case 8: case 9: case 10:
				extra = (extra << 4) | parse_hex(uch);
				estate++;
				break;
			case 5:
				extra = (extra << 4) | parse_hex(uch);
				if((extra & 0xFC00) == 0xDC00)
					throw error(ERR_INVALID_SURROGATE);
				else if((extra & 0xFC00) == 0xD800)
					estate = 6;
				else if((extra & 0xFFFE) == 0xFFFE)
					throw error(ERR_ILLEGAL_CHARACTER);
				else {
					estate = 0;
					*target = extra;
					++target;
				}
				break;
			case 6:
				if(uch != '\\')
					throw error(ERR_INVALID_SURROGATE);
				estate = 7;
				break;
			case 7:
				if(uch != 'u')
					throw error(ERR_INVALID_SURROGATE);
				estate = 8;
				break;
			case 11:
				extra = (extra << 4) | parse_hex(uch);
				if((extra & 0xFC00FC00UL) != 0xD800DC00)
					throw error(ERR_INVALID_SURROGATE);
				tmp = ((extra & 0x3FF0000) >> 6) + (extra & 0x3FF) + 0x10000;
				if((tmp & 0xFFFE) == 0xFFFE)
					throw error(ERR_ILLEGAL_CHARACTER);
				*target = tmp;
				++target;
				estate = 0;
				break;
			};
		}
		throw error(ERR_TRUNCATED_STRING);
	out:
		return i;
	}

	void read_string(std::u32string& target, const std::string& doc, size_t& ptr, size_t len)
	{
		size_t cpcnt;
		read_string_impl(iterator_counter(cpcnt), doc, ptr, len);
		target.resize(cpcnt);
		ptr = read_string_impl(target.begin(), doc, ptr, len) + 1;
	}

	json_token parse_token(const std::string& doc, size_t& ptr, size_t len)
	{
		if(ptr < len && (doc[ptr] == ' ' || doc[ptr] == '\t' || doc[ptr] == '\r' || doc[ptr] == '\n'))
			ptr++;
		if(ptr >= len)
			return json_token(json_token::TEOF);
		if(doc[ptr] == '{') {
			ptr++;
			return json_token(json_token::TOBJECT);
		}
		if(doc[ptr] == '}') {
			ptr++;
			return json_token(json_token::TOBJECT_END);
		}
		if(doc[ptr] == '[') {
			ptr++;
			return json_token(json_token::TARRAY);
		}
		if(doc[ptr] == ']') {
			ptr++;
			return json_token(json_token::TARRAY_END);
		}
		if(doc[ptr] == ',') {
			ptr++;
			return json_token(json_token::TCOMMA);
		}
		if(doc[ptr] == ':') {
			ptr++;
			return json_token(json_token::TCOLON);
		}
		if(doc[ptr] == '\"') {
			//String.
			ptr++;
			return json_token(json_token::TSTRING);
		}
		if(doc[ptr] == '-' || (doc[ptr] >= '0' && doc[ptr] <= '9')) {
			//Number.
			return json_token(json_token::TNUMBER);
		}
		if(doc[ptr] == 'n') {
			if(ptr >= len || doc[ptr++] != 'n') goto bad;
			if(ptr >= len || doc[ptr++] != 'u') goto bad;
			if(ptr >= len || doc[ptr++] != 'l') goto bad;
			if(ptr >= len || doc[ptr++] != 'l') goto bad;
			return json_token(json_token::TNULL);
		}
		if(doc[ptr] == 'f') {
			if(ptr >= len || doc[ptr++] != 'f') goto bad;
			if(ptr >= len || doc[ptr++] != 'a') goto bad;
			if(ptr >= len || doc[ptr++] != 'l') goto bad;
			if(ptr >= len || doc[ptr++] != 's') goto bad;
			if(ptr >= len || doc[ptr++] != 'e') goto bad;
			return json_token(json_token::TFALSE);
		}
		if(doc[ptr] == 't') {
			if(ptr >= len || doc[ptr++] != 't') goto bad;
			if(ptr >= len || doc[ptr++] != 'r') goto bad;
			if(ptr >= len || doc[ptr++] != 'u') goto bad;
			if(ptr >= len || doc[ptr++] != 'e') goto bad;
			return json_token(json_token::TTRUE);
		}
bad:
		return json_token(json_token::TINVALID);
	};


	const char* hexes = "0123456789abcdef";

	std::string json_string_escape(const std::u32string& c)
	{
		std::ostringstream out;
		out << "\"";
		size_t len = c.length();
		for(size_t i = 0; i < len; i++) {
			if(c[i] == '\b') out << "\\b";
			else if(c[i] == '\n') out << "\\n";
			else if(c[i] == '\r') out << "\\r";
			else if(c[i] == '\t') out << "\\t";
			else if(c[i] == '\f') out << "\\f";
			else if((c[i] & 0xFFFFFFE0) == 0)
				out << "\\u00" << hexes[c[i] >> 4] << hexes[c[i] % 16];
			else if(c[i] == U'\\')
				out << "\\\\";
			else if(c[i] == U'\"')
				out << "\\\"";
			else if(c[i] < 0x80)
				out << (unsigned char)c[i];
			else if(c[i] < 0x800) {
				out << (unsigned char)(0xC0 + (c[i] >> 6));
				out << (unsigned char)(0x80 + (c[i] & 0x3F));
			} else if(c[i] < 0x10000) {
				out << (unsigned char)(0xE0 + (c[i] >> 12));
				out << (unsigned char)(0x80 + ((c[i] >> 6) & 0x3F));
				out << (unsigned char)(0x80 + (c[i] & 0x3F));
			} else if(c[i] < 0x10FFFF) {
				out << (unsigned char)(0xF0 + (c[i] >> 18));
				out << (unsigned char)(0x80 + ((c[i] >> 12) & 0x3F));
				out << (unsigned char)(0x80 + ((c[i] >> 6) & 0x3F));
				out << (unsigned char)(0x80 + (c[i] & 0x3F));
			}
		}
		out << "\"";
		return out.str();
	}

	void skip_ws(const std::string& doc, size_t& ptr, size_t len) {
		while(ptr < len && (doc[ptr] == ' ' || doc[ptr] == '\t' || doc[ptr] == '\v' || doc[ptr] == '\r' ||
			doc[ptr] == '\n')) {
			ptr++;
		}
	}
}

std::string node::serialize() const throw(std::bad_alloc, error)
{
	std::ostringstream out;
	bool first = true;
	switch(vtype) {
	case null_tag::id: return "null";
	case boolean_tag::id: return _boolean ? "true" : "false";
	case number_tag::id:
		_number.write(out);
		return out.str();
	case string_tag::id:
		return json_string_escape(_string);
	case array_tag::id:
		out << "[";
		for(auto& i : xarray_index) {
			if(!first) out << ",";
			out << i->serialize();
			first = false;
		}
		out << "]";
		return out.str();
	case object_tag::id:
		out << "{";
		for(auto& i : xobject) {
			for(auto& j : i.second) {
				if(!first) out << ",";
				out << json_string_escape(i.first) << ":" << j.serialize();
				first = false;
			}
		}
		out << "}";
		return out.str();
	}
	throw error(ERR_UNKNOWN_TYPE);
}

node::node(const std::string& doc) throw(std::bad_alloc, error)
{
	size_t tmp = 0;
	ctor(doc, tmp, doc.length());
	skip_ws(doc, tmp, doc.length());
	if(tmp < doc.length())
		throw error(ERR_GARBAGE_AFTER_END);
}

void node::ctor(const std::string& doc, size_t& ptr, size_t len) throw(std::bad_alloc, error)
{
	json_token t = parse_token(doc, ptr, len);
	size_t tmp = ptr;
	std::string tmp2;
	switch(t.type) {
	case json_token::TTRUE:
		set(boolean, true);
		return;
	case json_token::TFALSE:
		set(boolean, false);
		return;
	case json_token::TNULL:
		set(null);
		return;
	case json_token::TEOF:
		throw error(ERR_TRUNCATED_JSON);
	case json_token::TCOMMA:
		throw error(ERR_UNEXPECTED_COMMA);
	case json_token::TCOLON:
		throw error(ERR_UNEXPECTED_COLON);
	case json_token::TARRAY_END:
		throw error(ERR_UNEXPECTED_RIGHT_BRACKET);
	case json_token::TOBJECT_END:
		throw error(ERR_UNEXPECTED_RIGHT_BRACE);
	case json_token::TSTRING:
		set(string, U"");
		read_string(_string, doc, ptr, len);
		break;
	case json_token::TNUMBER:
		set(number, 0.0);
		_number = number_holder(doc, ptr, len);
		break;
	case json_token::TOBJECT:
		*this = object();
		if(parse_token(doc, tmp, len).type == json_token::TOBJECT_END) {
			ptr = tmp;
			break;
		}
		while(true) {
			json_token t2 = parse_token(doc, ptr, len);
			if(t2.type == json_token::TEOF)
				throw error(ERR_TRUNCATED_JSON);
			if(t2.type != json_token::TSTRING)
				throw error(ERR_EXPECTED_STRING_KEY);
			std::u32string key;
			read_string(key, doc, ptr, len);
			t2 = parse_token(doc, ptr, len);
			if(t2.type == json_token::TEOF)
				throw error(ERR_TRUNCATED_JSON);
			if(t2.type != json_token::TCOLON)
				throw error(ERR_EXPECTED_COLON);
			insert(key, node(doc, ptr, len));
			t2 = parse_token(doc, ptr, len);
			if(t2.type == json_token::TEOF)
				throw error(ERR_TRUNCATED_JSON);
			if(t2.type == json_token::TOBJECT_END)
				break;
			if(t2.type != json_token::TCOMMA)
				throw error(ERR_EXPECTED_COMMA);
		}
		break;
	case json_token::TARRAY:
		*this = array();
		if(parse_token(doc, tmp, len).type == json_token::TARRAY_END) {
			ptr = tmp;
			break;
		}
		while(true) {
			append(node(doc, ptr, len));
			json_token t2 = parse_token(doc, ptr, len);
			if(t2.type == json_token::TEOF)
				throw error(ERR_TRUNCATED_JSON);
			if(t2.type == json_token::TARRAY_END)
				break;
			if(t2.type != json_token::TCOMMA)
				throw error(ERR_EXPECTED_COMMA);
		}
		break;
	case json_token::TINVALID:
		throw error(ERR_UNKNOWN_CHARACTER);
	}
}

node::node(const std::string& doc, size_t& ptr, size_t len) throw(std::bad_alloc, error)
{
	ctor(doc, ptr, len);
}

node& node::operator[](const std::u32string& pointer) throw(std::bad_alloc, error)
{
	node* current = this;
	size_t ptr = 0;
	while(ptr < pointer.length()) {
		size_t p = pointer.find_first_of(U"/", ptr);
		if(p > pointer.length())
			p = pointer.length();
		std::u32string c = jsonptr_unescape(pointer, ptr, p);
		if(current->vtype == array) {
			if(c == U"-") {
				//End-of-array.
				if(p < pointer.length())
					throw error(ERR_POINTER_BAD_APPEND);
				return current->append(n());
			}
			size_t idx;
			try {
				idx = parse_value<size_t>(to_u8string(c));
			} catch(std::bad_alloc& e) {
				throw;
			} catch(...) {
				throw error(ERR_POINTER_BAD_INDEX);
			}
			if(idx > current->xarray.size())
				throw error(ERR_POINTER_BAD_APPEND);
			else if(idx == current->xarray.size())
				return current->append(n());
			current = &current->index(idx);
		} else if(current->vtype == object) {
			if(!current->field_exists(c) && p == pointer.length())
				return current->insert(c, n());
			current = &current->field(c);
		} else
			throw error(ERR_NOT_ARRAY_NOR_OBJECT);
		ptr = p + 1;
	}
	return *current;
}

node& node::insert_node(const std::u32string& pointer, const node& nwn) throw(std::bad_alloc, error)
{
	size_t s = pointer.find_last_of(U"/");
	node* base;
	std::u32string rest;
	size_t ptrlen = pointer.length();
	if(s < ptrlen) {
		base = &follow(pointer.substr(0, s));
		rest = jsonptr_unescape(pointer, s + 1, ptrlen);
	} else {
		base = this;
		rest = jsonptr_unescape(pointer, 0, ptrlen);
	}
	if(base->type() == array) {
		if(rest == U"-")
			return base->append(nwn);
		size_t idx;
		try {
			idx = parse_value<size_t>(to_u8string(rest));
		} catch(std::bad_alloc& e) {
			throw;
		} catch(...) {
			throw error(ERR_POINTER_BAD_INDEX);
		}
		if(idx > base->xarray.size())
			throw error(ERR_POINTER_BAD_APPEND);
		else if(idx == base->xarray.size())
			return base->append(nwn);
		bool p = false;
		try {
			base->xarray.push_back(nwn);
			p = true;
			node* ptr = &*base->xarray.rbegin();
			base->xarray_index.insert(base->xarray_index.begin() + idx, ptr);
			return *ptr;
		} catch(std::bad_alloc& e) {
			if(p)
				base->xarray.pop_back();
			throw;
		}
	} else if(base->type() == object) {
		if(xobject.count(rest))
			return *base->xobject[rest].begin() = nwn;
		else {
			try {
				base->xobject[rest].push_back(nwn);
				return *base->xobject[rest].begin();
			} catch(...) {
				base->xobject.erase(rest);
				throw;
			}
		}		
	} else
		throw error(ERR_NOT_ARRAY_NOR_OBJECT);
}

node node::delete_node(const std::u32string& pointer) throw(std::bad_alloc, error)
{
	size_t s = pointer.find_last_of(U"/");
	node* base;
	std::u32string rest;
	size_t ptrlen = pointer.length();
	if(s < ptrlen) {
		base = &follow(pointer.substr(0, s));
		rest = jsonptr_unescape(pointer, s + 1, ptrlen);
	} else {
		base = this;
		rest = jsonptr_unescape(pointer, 0, ptrlen);
	}
	if(base->type() == array) {
		if(rest == U"-")
			throw error(ERR_POINTER_BAD_APPEND);
		size_t idx;
		try {
			idx = parse_value<size_t>(to_u8string(rest));
		} catch(std::bad_alloc& e) {
			throw;
		} catch(...) {
			throw error(ERR_POINTER_BAD_INDEX);
		}
		if(idx >= base->xarray.size())
			throw error(ERR_INDEX_INVALID);
		node* dptr = base->xarray_index[idx];
		node tmp = *dptr;
		for(auto i = base->xarray.begin(); i != base->xarray.end(); ++i)
			if(&*i == dptr) {
				base->xarray.erase(i);
				break;
			}
		base->xarray_index.erase(base->xarray_index.begin() + idx);
		return tmp;
	} else if(base->type() == object) {
		if(xobject.count(rest)) {
			node tmp = *base->xobject[rest].begin();
			base->xobject.erase(rest);
			return tmp;
		} else
			throw error(ERR_KEY_INVALID);
	} else
		throw error(ERR_NOT_ARRAY_NOR_OBJECT);
}

node node::patch(const node& patch) const throw(std::bad_alloc, error)
{
	node obj(*this);
	if(patch.type() != array)
		throw error(ERR_PATCH_BAD);
	for(auto& i : patch) {
		if(i.type() != object || i.field_count(U"op") != 1 || i.field(U"op").type() != string)
			throw error(ERR_PATCH_BAD);
		std::u32string op = i.field(U"op").as_string();
		if(op == U"test") {
			if(i.field_count(U"path") != 1 || i.field(U"path").type() != string)
				throw error(ERR_PATCH_BAD);
			if(i.field_count(U"value") != 1)
				throw error(ERR_PATCH_BAD);
			if(obj.follow(i.field(U"path").as_string()) != i.field("value"))
				throw error(ERR_PATCH_TEST_FAILED);
		} else if(op == U"remove") {
			if(i.field_count(U"path") != 1 || i.field(U"path").type() != string)
				throw error(ERR_PATCH_BAD);
			obj.delete_node(i.field(U"path").as_string());
		} else if(op == U"add") {
			if(i.field_count(U"path") != 1 || i.field(U"path").type() != string)
				throw error(ERR_PATCH_BAD);
			if(i.field_count(U"value") != 1)
				throw error(ERR_PATCH_BAD);
			obj.insert_node(i.field(U"path").as_string(), i.field("value"));
		} else if(op == U"replace") {
			if(i.field_count(U"path") != 1 || i.field(U"path").type() != string)
				throw error(ERR_PATCH_BAD);
			if(i.field_count(U"value") != 1)
				throw error(ERR_PATCH_BAD);
			obj.delete_node(i.field(U"path").as_string());
			obj.insert_node(i.field(U"path").as_string(), i.field("value"));
		} else if(op == U"move") {
			if(i.field_count(U"from") != 1 || i.field(U"from").type() != string)
				throw error(ERR_PATCH_BAD);
			if(i.field_count(U"path") != 1 || i.field(U"path").type() != string)
				throw error(ERR_PATCH_BAD);
			std::u32string from = i.field(U"from").as_string();
			std::u32string to = i.field(U"path").as_string();
			if(to.substr(0, from.length()) == from) {
				if(to.length() == from.length())
					continue;
				if(to.length() > from.length() && to[from.length()] == U'/')
					throw error(ERR_PATCH_ILLEGAL_MOVE);
			}
			node tmp = obj.delete_node(from);
			obj.insert_node(to, tmp);
		} else if(op == U"copy") {
			if(i.field_count(U"from") != 1 || i.field(U"from").type() != string)
				throw error(ERR_PATCH_BAD);
			if(i.field_count(U"path") != 1 || i.field(U"path").type() != string)
				throw error(ERR_PATCH_BAD);
			const node& tmp = obj.follow(i.field(U"from").as_string());
			obj.insert_node(i.field(U"path").as_string(), tmp);
		} else
			throw error(ERR_PATCH_BAD);
	}
	return obj;
}


node::iterator::iterator() throw() { n = NULL; }

node::iterator::iterator(node& _n) throw(error)
{
	n = &_n;
	idx = 0;
	if(n->type() == object) {
		if(n->xobject.empty())
			n = NULL;
		else
			_key = n->xobject.begin()->first;
	} else if(n->type() == array) {
		if(n->xarray.empty())
			n = NULL;
	} else
		throw error(ERR_NOT_ARRAY_NOR_OBJECT);
}

std::u32string node::iterator::key() throw(std::bad_alloc, error)
{
	if(!n)
		throw error(ERR_ITERATOR_END);
	return (n->type() == object) ? _key : U"";
}

size_t node::iterator::index() throw(error)
{
	if(!n)
		throw error(ERR_ITERATOR_END);
	return idx;
}

node& node::iterator::operator*() throw(error)
{
	if(!n)
		throw error(ERR_ITERATOR_END);
	if(n->type() == object) {
		if(!n->xobject.count(_key))
			throw error(ERR_ITERATOR_DELETED);
		auto& l = n->xobject.find(_key)->second;
		size_t j = 0;
		for(auto i = l.begin(); i != l.end(); i++, j++) {
			if(j == idx)
				return *i;
		}
		throw error(ERR_ITERATOR_DELETED);
	} else {
		if(idx >= n->xarray.size())
			throw error(ERR_ITERATOR_DELETED);
		return *n->xarray_index[idx];
	}
}

node* node::iterator::operator->() throw(error)
{
	return &**this;
}

node::iterator node::iterator::operator++(int) throw(error)
{
	iterator tmp = *this;
	++*this;
	return tmp;
}

node::iterator& node::iterator::operator++() throw(error)
{
	if(!n)
		throw error(ERR_ITERATOR_END);
	idx++;
	if(n->type() == object) {
		if(!n->xobject.count(_key) || n->xobject.find(_key)->second.size() <= idx) {
			auto i = n->xobject.upper_bound(_key);
			if(i == n->xobject.end())
				n = NULL;
			else
				_key = i->first;
			idx = 0;
		}
	} else {
		if(idx >= n->xarray_index.size())
			n = NULL;
	}
	return *this;
}

bool node::iterator::operator==(const iterator& i) throw()
{
	if(n != i.n)
		return false;
	if(!n && !i.n)
		return true;
	return (n == i.n && _key == i._key && idx == i.idx);
}

bool node::iterator::operator!=(const iterator& i) throw() { return !(*this == i); }

node::const_iterator::const_iterator() throw() { n = NULL; }

node::const_iterator::const_iterator(const node& _n) throw(error)
{
	n = &_n;
	idx = 0;
	if(n->type() == object) {
		if(n->xobject.empty())
			n = NULL;
		else
			_key = n->xobject.begin()->first;
	} else if(n->type() == array) {
		if(n->xarray.empty())
			n = NULL;
	} else
		throw error(ERR_NOT_ARRAY_NOR_OBJECT);
}

std::u32string node::const_iterator::key() throw(std::bad_alloc, error)
{
	if(!n)
		throw error(ERR_ITERATOR_END);
	return (n->type() == object) ? _key : U"";
}

size_t node::const_iterator::index() throw(error)
{
	if(!n)
		throw error(ERR_ITERATOR_END);
	return idx;
}

const node& node::const_iterator::operator*() throw(error)
{
	if(!n)
		throw error(ERR_ITERATOR_END);
	if(n->type() == object) {
		if(!n->xobject.count(_key))
			throw error(ERR_ITERATOR_DELETED);
		auto& l = n->xobject.find(_key)->second;
		size_t j = 0;
		for(auto i = l.begin(); i != l.end(); i++, j++) {
			if(j == idx)
				return *i;
		}
		throw error(ERR_ITERATOR_DELETED);
	} else {
		if(idx >= n->xarray.size())
			throw error(ERR_ITERATOR_DELETED);
		return *n->xarray_index[idx];
	}
}

const node* node::const_iterator::operator->() throw(error)
{
	return &**this;
}

node::const_iterator node::const_iterator::operator++(int) throw(error)
{
	const_iterator tmp = *this;
	++*this;
	return tmp;
}

node::const_iterator& node::const_iterator::operator++() throw(error)
{
	if(!n)
		throw error(ERR_ITERATOR_END);
	idx++;
	if(n->type() == object) {
		if(!n->xobject.count(_key) || n->xobject.find(_key)->second.size() <= idx) {
			auto i = n->xobject.upper_bound(_key);
			if(i == n->xobject.end())
				n = NULL;
			else
				_key = i->first;
			idx = 0;
		}
	} else {
		if(idx >= n->xarray_index.size())
			n = NULL;
	}
	return *this;
}

bool node::const_iterator::operator==(const const_iterator& i) throw()
{
	if(n != i.n)
		return false;
	if(!n && !i.n)
		return true;
	return (n == i.n && _key == i._key && idx == i.idx);
}

bool node::const_iterator::operator!=(const const_iterator& i) throw() { return !(*this == i); }

void node::erase_index(size_t idx) throw(error)
{
	if(type() == array) {
		if(idx >= xarray_index.size())
			return;
		node* n = xarray_index[idx];
		xarray_index.erase(xarray_index.begin() + idx);
		for(auto i = xarray.begin(); i != xarray.end(); i++)
			if(&*i == n) {
				xarray.erase(i);
				return;
			}
	} else
		throw error(ERR_NOT_AN_ARRAY);
}

void node::erase_field(const std::u32string& fld, size_t idx) throw(error)
{
	if(type() == object) {
		if(xobject.count(fld)) {
			auto& l = xobject[fld];
			size_t j = 0;
			for(auto i = l.begin(); i != l.end(); i++, j++)
				if(j == idx) {
					l.erase(i);
					break;
				}
			if(l.empty())
				xobject.erase(fld);
		}
	} else
		throw error(ERR_NOT_AN_OBJECT);
}

void node::erase_field_all(const std::u32string& fld) throw(error)
{
	if(type() == object) {
		if(xobject.count(fld))
			xobject.erase(fld);
	} else
		throw error(ERR_NOT_AN_OBJECT);
}

void node::clear() throw(error)
{
	if(type() == object)
		xobject.clear();
	else if(type() == array) {
		xarray_index.clear();
		xobject.clear();
	} else
		throw error(ERR_NOT_ARRAY_NOR_OBJECT);
}

node::iterator node::erase(node::iterator itr) throw(error)
{
	if(itr.n != this)
		throw error(ERR_WRONG_OBJECT);
	if(type() == object) {
		erase_field(itr._key, itr.idx);
		if(!xobject.count(itr._key) || itr.idx >= xobject[itr._key].size())
			itr++;
		return itr;
	} else if(type() == array) {
		erase_index(itr.idx);
		if(itr.idx >= xarray_index.size())
			itr.n = NULL;
		return itr;
	} else
		throw error(ERR_NOT_ARRAY_NOR_OBJECT);
}

void node::fixup_nodes(const node& _node)
{
	auto i = xarray.begin();
	auto j = _node.xarray.begin();
	for(; i != xarray.end(); i++, j++) {
		for(size_t k = 0; k < _node.xarray_index.size(); k++)
			if(_node.xarray_index[k] == &*j)
				xarray_index[k] = &*i;
	}
}

bool node::operator==(const node& n) const
{
	if(this == &n)
		return true;
	if(vtype != n.vtype)
		return false;
	switch(vtype) {
	case null_tag::id:
		return true;
	case boolean_tag::id:
		return (_boolean == n._boolean);
	case number_tag::id:
		return (_number == n._number);
	case string_tag::id:
		return (_string == n._string);
	case array_tag::id:
		if(xarray_index.size() != n.xarray_index.size())
			return false;
		for(size_t i = 0; i < xarray_index.size(); i++)
			if(*xarray_index[i] != *n.xarray_index[i])
				return false;
		return true;
	case object_tag::id:
		for(auto& i : xobject)
			if(!n.xobject.count(i.first))
				return false;
		for(auto& i : n.xobject)
			if(!xobject.count(i.first))
				return false;
		for(auto& i : xobject) {
			auto& j = *xobject.find(i.first);
			auto& k = *n.xobject.find(i.first);
			if(j.second.size() != k.second.size())
				return false;
			auto j2 = j.second.begin();
			auto k2 = k.second.begin();
			for(; j2 != j.second.end(); j2++, k2++)
				if(*j2 != *k2)
					return false;
		}
		return true;
	default:
		throw error(ERR_UNKNOWN_TYPE);
	}
}
}

bool operator==(const int& n, const JSON::number_tag& v) { return v == n; }
bool operator==(const int& n, const JSON::string_tag& v) { return v == n; }
bool operator==(const int& n, const JSON::boolean_tag& v) { return v == n; }
bool operator==(const int& n, const JSON::array_tag& v) { return v == n; }
bool operator==(const int& n, const JSON::object_tag& v) { return v == n; }
bool operator==(const int& n, const JSON::null_tag& v) { return v == n; }
bool operator!=(const int& n, const JSON::number_tag& v) { return v != n; }
bool operator!=(const int& n, const JSON::string_tag& v) { return v != n; }
bool operator!=(const int& n, const JSON::boolean_tag& v) { return v != n; }
bool operator!=(const int& n, const JSON::array_tag& v) { return v != n; }
bool operator!=(const int& n, const JSON::object_tag& v) { return v != n; }
bool operator!=(const int& n, const JSON::null_tag& v) { return v != n; }

