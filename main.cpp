#include <iostream>
#include <ctime>
#include <vector>
#include <string>
#include <sqlite3.h>

using namespace std;

/*
Banking system implemented in C++ 
with classes for customers, accounts, transactions, and banking services. 

Features included creating and managing customers, accounts, and transactions, 
as well as performing banking services such as withdrawals, deposits, and transfers. 

It also allows customers to view their account information, including account balances, 
recent transactions, and other details.
*/

class Customer {
    private:
        int customerId; 
        string name;
        string email;
        string phone;
    public:
        Customer(int id, string name, string email, string phone)
        : customerId(id),
          name(name),
          email(email),
          phone(phone) {  
          }

        Customer() : customerId(0), name(""), email(""), phone("") {}

        // Getters and setters for customer information
        int getCustomerId() {
            return customerId;
        }

        string getName() {
            return name;
        }

        void setName(string newName) {
            name = newName;
        }

        string getEmail() {
            return email;
        }

        void setEmail(string newEmail) {
            email = newEmail;
        }

        string getPhone() {
            return phone;
        }

        void setPhone(string newPhone) {
            phone = newPhone;
        }

        void displayCustomerInfo() {
            cout << "Customer ID: " << customerId << endl;
            cout << "Name: " << name << endl;
            cout << "Email: " << email << endl;
            cout << "Phone: " << phone << endl;
        }
};


enum TransactionType {
    DEPOSIT,
    WITHDRAWAL,
    TRANSFER,
};

// Converts a TransactionType to/from the text stored in the database.
string transactionTypeToString(TransactionType type) {
    switch (type) {
        case DEPOSIT:    return "DEPOSIT";
        case WITHDRAWAL: return "WITHDRAWAL";
        case TRANSFER:   return "TRANSFER";
    }
    return "UNKNOWN";
}

TransactionType transactionTypeFromString(const string& type) {
    if (type == "WITHDRAWAL") return WITHDRAWAL;
    if (type == "TRANSFER")   return TRANSFER;
    return DEPOSIT;
}

class Transaction {
    private:
        int transactionId;
        TransactionType transactionType;
        double amount;
        time_t timestamp;
    public:
        Transaction(int id, TransactionType transactionType, double amount) :
            transactionId(id),
            transactionType(transactionType),
            amount(amount),
            timestamp(time(0)) {};

        // Restores a transaction loaded from the database, preserving its
        // original timestamp.
        Transaction(int id, TransactionType transactionType, double amount,
                    time_t timestamp) :
            transactionId(id),
            transactionType(transactionType),
            amount(amount),
            timestamp(timestamp) {};

        int getTransactionId() {
            return transactionId;
        }

        TransactionType getTransactionType() {
            return transactionType;
        }

        double getAmount() {
            return amount;
        }

        time_t getTimestamp() {
            return timestamp;
        }

        void displayTransactionInfo() {
            cout << "Transaction ID: " << transactionId << endl;
            cout << "Transaction Type: " << transactionType << endl;
            cout << "Amount: $" << amount << endl;
            cout << "Timestamp: " << ctime(&timestamp) << endl;
        }
};


class Account {
    private:
        int accountNumber;
        int customerId;
        double balance;
        vector<Transaction> transactions;
    public:
        Account(int accountNumber, int customerId, double balance, vector<Transaction> transactions) :
            accountNumber(accountNumber),
            customerId(customerId),
            balance(balance),
            transactions(transactions) {};

        Account(int accountNumber, int customerId, double balance) :
            accountNumber(accountNumber),
            customerId(customerId),
            balance(balance),
            transactions({}) {};

        // Getetrs and setters for account information
        int getAccountNumber() {
            return accountNumber;
        }

        int getCustomerId() {
            return customerId;
        }

        double getBalance() {
            return balance;
        }

        vector<Transaction> getTransactions() {
            return transactions;
        }

        // Methods for performing banking services
        void deposit(int transactionId, double amount) {
            balance += amount;
            Transaction transaction(transactionId, DEPOSIT, amount);
            transactions.push_back(transaction);
        }

        // Withdraws the given amount, recording the transaction.
        // Returns false (without modifying the account) if the amount is
        // invalid or there are insufficient funds.
        bool withdraw(int transactionId, double amount) {
            if (amount <= 0 || amount > balance) {
                return false;
            }
            balance -= amount;
            Transaction transaction(transactionId, WITHDRAWAL, amount);
            transactions.push_back(transaction);
            return true;
        }

        void displayAccountInfo() {
            cout << "Account Number: " << accountNumber << endl;
            cout << "Customer ID: " << customerId << endl;
            cout << "Balance: $" << balance << endl;
        }

};

// Thin wrapper around a SQLite connection that persists customers, accounts,
// and transactions. The BankingSystem keeps the authoritative in-memory model
// and writes through to this database so the data survives across runs.
class Database {
    private:
        sqlite3* db = nullptr;

        void logError(const string& context) {
            cerr << "Database error (" << context << "): "
                 << sqlite3_errmsg(db) << endl;
        }

        // Runs a statement that returns no rows (DDL, BEGIN/COMMIT, etc.).
        bool exec(const string& sql) {
            char* errMsg = nullptr;
            if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg)
                    != SQLITE_OK) {
                cerr << "Database error (exec): " << errMsg << endl;
                sqlite3_free(errMsg);
                return false;
            }
            return true;
        }

    public:
        // Opens (or creates) the database file and ensures the schema exists.
        Database(const string& path) {
            if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
                logError("open");
            }
            exec("PRAGMA foreign_keys = ON;");
            createSchema();
        }

        ~Database() {
            if (db != nullptr) {
                sqlite3_close(db);
            }
        }

        void createSchema() {
            exec(
                "CREATE TABLE IF NOT EXISTS customers ("
                "  customer_id INTEGER PRIMARY KEY,"
                "  name        TEXT NOT NULL,"
                "  email       TEXT,"
                "  phone       TEXT"
                ");"
                "CREATE TABLE IF NOT EXISTS accounts ("
                "  account_number INTEGER PRIMARY KEY,"
                "  customer_id    INTEGER NOT NULL,"
                "  balance        REAL NOT NULL DEFAULT 0,"
                "  FOREIGN KEY (customer_id) REFERENCES customers(customer_id)"
                ");"
                "CREATE TABLE IF NOT EXISTS transactions ("
                "  transaction_id INTEGER PRIMARY KEY,"
                "  account_number INTEGER NOT NULL,"
                "  type           TEXT NOT NULL,"
                "  amount         REAL NOT NULL,"
                "  timestamp      INTEGER NOT NULL,"
                "  FOREIGN KEY (account_number) REFERENCES accounts(account_number)"
                ");"
            );
        }

        bool beginTransaction() { return exec("BEGIN;"); }
        bool commit()           { return exec("COMMIT;"); }
        bool rollback()         { return exec("ROLLBACK;"); }

        // INSERT OR IGNORE so re-seeding an existing database is harmless.
        bool insertCustomer(int id, const string& name, const string& email,
                            const string& phone) {
            const char* sql = "INSERT OR IGNORE INTO customers "
                              "(customer_id, name, email, phone) "
                              "VALUES (?, ?, ?, ?);";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logError("insertCustomer");
                return false;
            }
            sqlite3_bind_int(stmt, 1, id);
            sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, phone.c_str(), -1, SQLITE_TRANSIENT);
            bool ok = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);
            return ok;
        }

        bool insertAccount(int accountNumber, int customerId, double balance) {
            const char* sql = "INSERT OR IGNORE INTO accounts "
                              "(account_number, customer_id, balance) "
                              "VALUES (?, ?, ?);";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logError("insertAccount");
                return false;
            }
            sqlite3_bind_int(stmt, 1, accountNumber);
            sqlite3_bind_int(stmt, 2, customerId);
            sqlite3_bind_double(stmt, 3, balance);
            bool ok = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);
            return ok;
        }

        bool updateBalance(int accountNumber, double balance) {
            const char* sql = "UPDATE accounts SET balance = ? "
                              "WHERE account_number = ?;";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logError("updateBalance");
                return false;
            }
            sqlite3_bind_double(stmt, 1, balance);
            sqlite3_bind_int(stmt, 2, accountNumber);
            bool ok = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);
            return ok;
        }

        bool insertTransaction(int transactionId, int accountNumber,
                               const string& type, double amount,
                               time_t timestamp) {
            const char* sql = "INSERT INTO transactions "
                              "(transaction_id, account_number, type, amount, timestamp) "
                              "VALUES (?, ?, ?, ?, ?);";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logError("insertTransaction");
                return false;
            }
            sqlite3_bind_int(stmt, 1, transactionId);
            sqlite3_bind_int(stmt, 2, accountNumber);
            sqlite3_bind_text(stmt, 3, type.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(stmt, 4, amount);
            sqlite3_bind_int64(stmt, 5, (sqlite3_int64)timestamp);
            bool ok = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);
            return ok;
        }

        // Highest transaction id stored so far (0 if none), used to continue
        // numbering after a reload.
        int maxTransactionId() {
            const char* sql = "SELECT COALESCE(MAX(transaction_id), 0) "
                              "FROM transactions;";
            sqlite3_stmt* stmt = nullptr;
            int result = 0;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    result = sqlite3_column_int(stmt, 0);
                }
            }
            sqlite3_finalize(stmt);
            return result;
        }

        vector<Customer> loadCustomers() {
            vector<Customer> customers;
            const char* sql = "SELECT customer_id, name, email, phone "
                              "FROM customers;";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logError("loadCustomers");
                return customers;
            }
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int id = sqlite3_column_int(stmt, 0);
                const unsigned char* name = sqlite3_column_text(stmt, 1);
                const unsigned char* email = sqlite3_column_text(stmt, 2);
                const unsigned char* phone = sqlite3_column_text(stmt, 3);
                customers.push_back(Customer(
                    id,
                    name  ? reinterpret_cast<const char*>(name)  : "",
                    email ? reinterpret_cast<const char*>(email) : "",
                    phone ? reinterpret_cast<const char*>(phone) : ""));
            }
            sqlite3_finalize(stmt);
            return customers;
        }

        // Loads every account along with its recorded transactions.
        vector<Account> loadAccounts() {
            vector<Account> accounts;
            const char* sql = "SELECT account_number, customer_id, balance "
                              "FROM accounts;";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logError("loadAccounts");
                return accounts;
            }
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int accountNumber = sqlite3_column_int(stmt, 0);
                int customerId = sqlite3_column_int(stmt, 1);
                double balance = sqlite3_column_double(stmt, 2);
                accounts.push_back(Account(accountNumber, customerId, balance,
                                           loadTransactions(accountNumber)));
            }
            sqlite3_finalize(stmt);
            return accounts;
        }

        vector<Transaction> loadTransactions(int accountNumber) {
            vector<Transaction> transactions;
            const char* sql = "SELECT transaction_id, type, amount, timestamp "
                              "FROM transactions WHERE account_number = ? "
                              "ORDER BY transaction_id;";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logError("loadTransactions");
                return transactions;
            }
            sqlite3_bind_int(stmt, 1, accountNumber);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int id = sqlite3_column_int(stmt, 0);
                const unsigned char* type = sqlite3_column_text(stmt, 1);
                double amount = sqlite3_column_double(stmt, 2);
                time_t timestamp = (time_t)sqlite3_column_int64(stmt, 3);
                transactions.push_back(Transaction(
                    id,
                    transactionTypeFromString(
                        type ? reinterpret_cast<const char*>(type) : ""),
                    amount,
                    timestamp));
            }
            sqlite3_finalize(stmt);
            return transactions;
        }
};

class BankingSystem {
    private:
        Database db;
        int currentTransactionId = 0;
        vector<Customer> customers;
        vector<Account> accounts;

        // Returns a pointer to the account with the given number, or
        // nullptr if no such account exists.
        Account* findAccount(int accountNumber) {
            for (Account& account : accounts) {
                if (account.getAccountNumber() == accountNumber) {
                    return &account;
                }
            }
            return nullptr;
        }

        // Persists the most recently recorded transaction of an account and
        // its updated balance to the database.
        void persistLastTransaction(Account* account) {
            vector<Transaction> txns = account->getTransactions();
            Transaction last = txns.back();
            db.insertTransaction(last.getTransactionId(),
                                 account->getAccountNumber(),
                                 transactionTypeToString(last.getTransactionType()),
                                 last.getAmount(),
                                 last.getTimestamp());
            db.updateBalance(account->getAccountNumber(), account->getBalance());
        }

    public:
        // Opens the database and loads any previously persisted data.
        BankingSystem(const string& dbPath) : db(dbPath) {
            customers = db.loadCustomers();
            accounts = db.loadAccounts();
            currentTransactionId = db.maxTransactionId();
        }

        int getAccountCount() {
            return (int)accounts.size();
        }

        void addCustomer(const Customer& customer) {
            customers.push_back(customer);
            Customer copy = customer;
            db.insertCustomer(copy.getCustomerId(), copy.getName(),
                              copy.getEmail(), copy.getPhone());
        }

        void addAccount(const Account& account) {
            accounts.push_back(account);
            Account copy = account;
            db.insertAccount(copy.getAccountNumber(), copy.getCustomerId(),
                             copy.getBalance());
        }

        // Deposits the amount into the given account. Returns true on success.
        bool deposit(int accountNumber, double amount) {
            Account* account = findAccount(accountNumber);
            if (account == nullptr) {
                cout << "Deposit failed: account " << accountNumber
                     << " not found." << endl;
                return false;
            }
            if (amount <= 0) {
                cout << "Deposit failed: amount must be positive." << endl;
                return false;
            }
            account->deposit(++currentTransactionId, amount);
            persistLastTransaction(account);
            cout << "Deposited $" << amount << " into account "
                 << accountNumber << "." << endl;
            return true;
        }

        // Withdraws the amount from the given account. Returns true on success.
        bool withdraw(int accountNumber, double amount) {
            Account* account = findAccount(accountNumber);
            if (account == nullptr) {
                cout << "Withdrawal failed: account " << accountNumber
                     << " not found." << endl;
                return false;
            }
            if (!account->withdraw(++currentTransactionId, amount)) {
                cout << "Withdrawal failed: invalid amount or insufficient "
                     << "funds in account " << accountNumber << "." << endl;
                return false;
            }
            persistLastTransaction(account);
            cout << "Withdrew $" << amount << " from account "
                 << accountNumber << "." << endl;
            return true;
        }

        // Transfers the amount from one account to another. The withdrawal and
        // deposit are recorded as separate transactions on each account and
        // committed to the database atomically. Returns true on success and
        // leaves both accounts unchanged on failure.
        bool transfer(int fromAccountNumber, int toAccountNumber, double amount) {
            Account* fromAccount = findAccount(fromAccountNumber);
            Account* toAccount = findAccount(toAccountNumber);
            if (fromAccount == nullptr || toAccount == nullptr) {
                cout << "Transfer failed: source or destination account "
                     << "not found." << endl;
                return false;
            }
            if (fromAccountNumber == toAccountNumber) {
                cout << "Transfer failed: cannot transfer to the same account."
                     << endl;
                return false;
            }
            if (!fromAccount->withdraw(++currentTransactionId, amount)) {
                cout << "Transfer failed: invalid amount or insufficient funds "
                     << "in account " << fromAccountNumber << "." << endl;
                return false;
            }
            toAccount->deposit(++currentTransactionId, amount);

            // Persist both legs of the transfer in a single DB transaction.
            db.beginTransaction();
            persistLastTransaction(fromAccount);
            persistLastTransaction(toAccount);
            db.commit();

            cout << "Transferred $" << amount << " from account "
                 << fromAccountNumber << " to account " << toAccountNumber
                 << "." << endl;
            return true;
        }

        void displayAccount(int accountNumber) {
            Account* account = findAccount(accountNumber);
            if (account == nullptr) {
                cout << "Account " << accountNumber << " not found." << endl;
                return;
            }
            account->displayAccountInfo();
        }
};


int main() {
    // All data is persisted to bank.db in the working directory and reloaded
    // on the next run.
    BankingSystem bank("bank.db");

    if (bank.getAccountCount() == 0) {
        cout << "Fresh database - seeding customers and accounts, then "
             << "running demo operations.\n" << endl;

        bank.addCustomer(Customer(1, "Alice", "alice@example.com", "555-0001"));
        bank.addCustomer(Customer(2, "Bob", "bob@example.com", "555-0002"));

        bank.addAccount(Account(1001, 1, 500.0));
        bank.addAccount(Account(1002, 2, 100.0));

        // Demonstrate deposit, withdrawal, and transfer.
        bank.deposit(1001, 250.0);
        bank.withdraw(1001, 100.0);
        bank.transfer(1001, 1002, 300.0);
    } else {
        cout << "Loaded " << bank.getAccountCount()
             << " account(s) from bank.db (data persisted from a previous run)."
             << endl;
    }

    cout << "\nFinal account states:" << endl;
    bank.displayAccount(1001);
    cout << endl;
    bank.displayAccount(1002);

    return 0;
}



