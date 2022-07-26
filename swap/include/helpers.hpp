#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <string>

using namespace eosio;
using std::string;

struct transfer_args {
    name from;
    name to;
    asset quantity;
    string memo;
};

struct account_balance {
    asset    balance;
    uint64_t primary_key() const { return balance.symbol.code().raw(); }
};

struct currency_stats {
    asset supply;
    asset max_supply;
    name issuer;
    uint64_t primary_key() const { return supply.symbol.code().raw(); }
};

typedef eosio::multi_index<"accounts"_n, account_balance > accounts;
typedef eosio::multi_index<"stat"_n, currency_stats> stats;


asset get_supply(const name &token_contract, const symbol_code &sym_code) {
   stats statstable(token_contract, sym_code.raw());
   string err_msg = "invalid token contract: ";
   err_msg.append(token_contract.to_string());
   const auto &st = statstable.require_find(sym_code.raw(), err_msg.c_str());
   return st->supply;
}

asset get_balance(const name &token_contract, const name &owner, const symbol &sym) {
    asset ret = asset( 0, sym );
    accounts accounts_table( token_contract, owner.value );
    auto accounts_it = accounts_table.find( sym.code().raw() );
    if ( accounts_it != accounts_table.end() ) {
        ret = accounts_it->balance;
    }
    return ret;
}

void inline_transfer(name contract, name from, name to, asset quantity, string memo) {
    auto data = std::make_tuple(from, to, quantity, memo);
    action(permission_level{from, "active"_n}, contract, "transfer"_n, data).send();
}

uint64_t str_hash(extended_symbol token0, extended_symbol token1) {
    string hash;
    auto ftoken = token0;
    auto stoken = token1;
    if (token0 < token1) {
        // reverse
        ftoken = token1;
        stoken = token0;
    }
    hash.append(ftoken.get_contract().to_string());
    hash.append("-");
    hash.append(ftoken.get_symbol().code().to_string());
    hash.append("-");
    hash.append(stoken.get_contract().to_string());
    hash.append("-");
    hash.append(stoken.get_symbol().code().to_string());
    return std::hash<string>{}(hash);
}

bool is_same_pair(extended_symbol token0, extended_symbol token1, extended_symbol token2, extended_symbol token3) {
    return (token0 == token2 && token1 == token3) || (token0 == token3 && token1 == token2);
}

std::vector<string> split(const string& str, const string& delim) {
    std::vector<string> vect;
    size_t prev = 0, pos = 0;
    do {
        pos = str.find(delim, prev);
        if (pos == string::npos) {
            pos = str.length();
        }
        string token = str.substr(prev, pos-prev);
        if (!token.empty()) {
            vect.push_back(token);
        }
        prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());
    return vect;
}

std::map<string, string> mappify(string const& s) {
    std::map<string, string> m;
    string::size_type key_pos = 0;
    string::size_type key_end;
    string::size_type val_pos;
    string::size_type val_end;
    while((key_end = s.find(':', key_pos)) != string::npos) {
        if ((val_pos = s.find_first_not_of(":", key_end)) == string::npos) break;
        val_end = s.find(',', val_pos);
        m.emplace(s.substr(key_pos, key_end - key_pos), s.substr(val_pos, val_end - val_pos));
        key_pos = val_end;
        if (key_pos != string::npos) ++key_pos;
    }
    return m;
}

string id2code(uint64_t id) {
    string code = "";
    while (id > 0) {
		int m = id % 26;
		if(m == 0) m = 26;
		code = (char)(m + 64) + code;
		id = (id - m) / 26;
    }
    return "LP" + code;
}

uint64_t code2id(string s) {
    uint64_t id = 0;
    for (int i = 0, j = 1; i < s.size(); i++, j *= 26) {
		id += (int)(s[s.size() - i - 1] - 64) * j;
	}
    return id;
}

template<typename ... Args>
std::string string_format(const std::string& format, Args ... args) {
    int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if (size_s <= 0) { return string(""); }
    auto size = static_cast<size_t>( size_s );
    std::unique_ptr<char[]> buf( new char[ size ] );
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}