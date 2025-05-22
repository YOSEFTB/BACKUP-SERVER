#include <boost/asio.hpp>
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <fstream>


/* This program implements a server in CPP. The program runs a new thread for every client. when a cvlient connects it can send messages to the server, requesting 
a file be restored to it, or backed up on the server, or deleted, or just to return a list of the clients files on the server. The server parses the clients message and gets the 
server ID, opcode and all necessary info to execute the operation. The server sends a response to the client, depending on the avction and if it was successful or not.
The server handles all types of errors- errors in connection, errors in format, and errors in file handling and non existing files. At the conclusion- the server ends its session with the client*/

using boost::asio::ip::tcp;
std::mutex cout_mutex;

// Read first 6 bytes (ID, version, opcode)
bool get_op(tcp::socket& socket, uint32_t& packet_id, uint8_t& version, uint8_t& opcode, boost::system::error_code& ec) {
    char buffer[6];
    try {
        boost::asio::read(socket, boost::asio::buffer(buffer, 6), ec);
    }
    catch (const boost::system::system_error& e) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Socket reading error: " << e.what() << std::endl;
    }
    if (ec) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Alert. While reading: " << ec.message() << " or end of client message." << std::endl;
        return false;  // Exit if connection is lost
    }
    std::memcpy(&packet_id, buffer, 4);
    version = buffer[4];
    opcode = buffer[5];
    return true;
}

void send_data(tcp::socket& socket, const std::vector<uint8_t>& data) {
    try {
        boost::asio::write(socket, boost::asio::buffer(data));
    }
    catch (const boost::system::system_error& e) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Error sending data: " << e.what() << std::endl;
    }
}

// Function to create and send header format with only version and status code
void send_format1(tcp::socket& socket, uint8_t version, uint16_t status) {
    std::vector<uint8_t> buffer;
    buffer.push_back(version);
    buffer.push_back(status & 0xFF);       // Low byte first
    buffer.push_back((status >> 8) & 0xFF); // High byte
    send_data(socket, buffer);
}

// Function to create and send header with version, status code, name_length, and file_name
void send_format2(tcp::socket& socket, uint8_t version, uint16_t status, const std::string& filename) {
    std::vector<uint8_t> buffer;
    buffer.push_back(version);
    buffer.push_back(status & 0xFF);
    buffer.push_back((status >> 8) & 0xFF);

    size_t filename_length = filename.length();
    uint16_t filename_length_short = static_cast<uint16_t>(filename_length);
    buffer.push_back(filename_length_short & 0xFF);
    buffer.push_back((filename_length_short >> 8) & 0xFF);

    buffer.insert(buffer.end(), filename.begin(), filename.end());
    send_data(socket, buffer);
}

// Function to create and send header version, status code, name_length, file_name, file_length, and file_content
void send_format3(tcp::socket& socket, uint8_t version, uint16_t status, const std::string& filename, const std::vector<uint8_t>& file_content) {
    std::vector<uint8_t> buffer;
    buffer.push_back(version);
    buffer.push_back(status & 0xFF);
    buffer.push_back((status >> 8) & 0xFF);

   // uint16_t filename_length = filename.length();
    size_t filename_length = filename.length();
    uint16_t filename_length_short = static_cast<uint16_t>(filename_length);
    buffer.push_back(filename_length_short & 0xFF);
    buffer.push_back((filename_length_short >> 8) & 0xFF);
    buffer.insert(buffer.end(), filename.begin(), filename.end());

    uint32_t file_length = file_content.size();
    buffer.push_back(file_length & 0xFF);
    buffer.push_back((file_length >> 8) & 0xFF);
    buffer.push_back((file_length >> 16) & 0xFF);
    buffer.push_back((file_length >> 24) & 0xFF);

    buffer.insert(buffer.end(), file_content.begin(), file_content.end());
    send_data(socket, buffer);
}

// Function to create and return a list of the clients files that are on server
std::vector<std::string> get_backup_list(const std::filesystem::path& path) {
    std::vector<std::string> vec;
    if (!std::filesystem::exists(path))
        return vec;
    else {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            vec.push_back(entry.path().filename().string());  // Add the filename to the vector
        }
        return vec;
    }
}

// Read filename (2-byte length + name)
std::string get_filename(tcp::socket& socket) {
    char len_buf[2];
    try {
        boost::asio::read(socket, boost::asio::buffer(len_buf, 2));
    }
    catch (const boost::system::system_error& e) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Error reading filename length" << e.what() << std::endl;
        return "";
    }

    uint16_t name_len;
    std::memcpy(&name_len, len_buf, 2); // Copy 2-byte length
    std::vector<char> name_buf(name_len);
    try {
        boost::asio::read(socket, boost::asio::buffer(name_buf, name_len));
    }
    catch (const boost::system::system_error& e) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Error reading filename: " << e.what() << std::endl;
        return "";
    }

    return std::string(name_buf.begin(), name_buf.end());
}

// Function to delete file from server
void delete_file(tcp::socket& socket, const std::filesystem::path& filepath,uint8_t version) {
    std::string file_name = filepath.filename().string();
    if(std::filesystem::exists(filepath)){
        try {
            std::filesystem::remove(filepath);
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "File at " << filepath << " deleted" << std::endl;
            send_format2(socket, version, 212, file_name);
        }
        catch (const std::filesystem::filesystem_error& e) {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "Error: " << e.what() << std::endl;
            send_format1(socket, version, 1003);

        }
    }
    else {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "File at " << filepath << "doesnt exist\n";
        send_format2(socket, version, 1001, file_name);
    }
}
  // Function to get actual file content (used for restoring)
std::vector<char> get_file_content(tcp::socket& socket) {
    char len_buf[4];
    try {
        boost::asio::read(socket, boost::asio::buffer(len_buf, 4));
    }
    catch (const boost::system::system_error& e) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Error reading file length: " << e.what() << std::endl;
        return {};
    }

    uint32_t content_len;
    std::memcpy(&content_len, len_buf, 4); // Copy 4-byte length
    std::vector<char> content_buf(content_len);
    try {
        boost::asio::read(socket, boost::asio::buffer(content_buf, content_len));
    }
    catch (const boost::system::system_error& e) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Error reading file content" << e.what() << std::endl;
        return {};
    }
    return std::vector<char>(content_buf.begin(), content_buf.end());
}

// Function to write data to the file (used for backing up users file) 
bool write_file(const std::filesystem::path& path, const std::vector<char>& content) {
    try {
        std::ofstream file(path, std::ios::binary); // Open file in binary mode
        if (!file) {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cerr << "Error: Could not open file for writing: " << path << std::endl;
            return false;
        }
        file.write(content.data(), content.size()); // Write content to file
        if (!file) {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cerr << "Error: Failed to write to file: " << path << std::endl;
            return false;
        }
        return true;
    }
    catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Error while writing file: " << e.what() << std::endl;
        return false;
    }
}

// Used for info about the file. 
bool get_file_info(const std::filesystem::path& filepath, std::vector <uint8_t>& content, size_t& filelength) {
    try {
        std::ifstream file(filepath, std::ios::in | std::ios::binary);
        if (!file) {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cerr << "Error: Unable to open file. File at " << filepath << " doesn't exist.\n";
            content.clear();
            return false;
        }
        // Read content and get the length
        content.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        filelength = content.size();
        return true;
    }
    catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Error while reading file: " << e.what() << std::endl;
        return false;
    }
}
// Generates a 32 byte string which will be the name of the file sent to user when he requests a list of backup files
std::string generateRandomString(size_t length) {
    const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string randomStr;
    std::srand(std::time(0));
    for (size_t i = 0; i < length; ++i) {
        randomStr += chars[std::rand() % chars.size()];
    }
    return randomStr;
}

// The function where the main logic of the program is executed- client handling, flow control, connection/reading handling, operation determining
void handle_client(tcp::socket socket) {
    try {
        while (true) {
            uint32_t client_id;
            uint8_t version, opcode;
            boost::system::error_code ec;
            if (!get_op(socket, client_id, version, opcode, ec)) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Client disconnected or error occurred. Closing connection.\n\n";
                break;  // Exit loop when the client disconnects
            }
            std::filesystem::path path = "C:\\backupsvr";
            std::filesystem::path name_path = path / std::to_string(client_id);
            if (!std::filesystem::exists(name_path)) {  // Check if the directory exists
                if (std::filesystem::create_directories(name_path)) {  // Create directory if it doesn't exist
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "Directory created for client at: " << name_path << std::endl;
                }
                else {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cerr << "Failed to create client folder: " << name_path << std::endl;
                }
            }

            if (opcode == 202) { //get list of files in server
                std::vector<std::string> vec = get_backup_list(name_path);
                //<<<<vec will be sent with other server info, back to the client>>>>
                if (vec.empty()) {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    uint16_t status = 1002;
                    std::string error_message = "There are no files to be found on the server for this client.\n";
                    std::cout << error_message;
                    send_format1(socket, version, 1002);
                }
                else {
                    std::string backups_file_name = generateRandomString(32);
                    std::ofstream outFile(backups_file_name);

                    // Check if the file is open
                    if (!outFile) {
                        std::lock_guard<std::mutex> lock(cout_mutex);
                        std::cerr << "Error opening file for writing!" << std::endl;
                        send_format1(socket, version, 1003);
                        continue;
                    }

                    // Write each string to the file
                    for (const std::string& str : vec) {
                        outFile << str << std::endl;; // Write content to file
                        if (!outFile) {
                            std::lock_guard<std::mutex> lock(cout_mutex);
                            std::cerr << "Error: Failed to write to file: " << path << std::endl;
                            send_format1(socket, version, 1003);
                            continue;
                        }
                    }
                    // read the content of the file into a vector of uint8_t
                    std::ifstream inFile(backups_file_name, std::ios::binary);
                    std::vector<uint8_t> file_content((std::istreambuf_iterator<char>(inFile)),
                        std::istreambuf_iterator<char>());

                    // Check if the file was read correctly
                    if (inFile.fail()) {
                        std::lock_guard<std::mutex> lock(cout_mutex);
                        std::cerr << "Error reading the file: " << backups_file_name << std::endl;
                        send_format1(socket, version, 1003);
                        continue;
                    }
                    std::cout << "Sent list of backup files to client.\n";
                    send_format3(socket, version, 211, backups_file_name, file_content);
                }
            }
            else if (opcode == 200) {  //restore a file
                std::vector<uint8_t> content;
                size_t file_length;
                std::string filename = get_filename(socket);
                std::filesystem::path full_file_path = name_path / filename;
                if (!get_file_info(full_file_path, content, file_length)) {
                    send_format2(socket, version, 1001, filename);
                    continue;
                }
                std::cout << "The file is restored at path " << full_file_path << std::endl;
                send_format3(socket, version, 210, filename, content);
            }

            else if (opcode == 201) {  //delete file from server
                std::string filename = get_filename(socket);
                std::filesystem::path full_file_path = name_path / filename;
                std::cout << "Attempting to delete file " << filename << ", at path- " << full_file_path.string() << std::endl;
                delete_file(socket, full_file_path, version);
            }

            else if (opcode == 100) { //save in backup server
                std::string filename = get_filename(socket);
                std::filesystem::path full_file_path = name_path / filename;
                std::vector<char> content = get_file_content(socket);
                if (write_file(full_file_path, content)) {
                    send_format2(socket, version, 212, filename);
                    std::cout << "The file " << filename << " was saved in the server, at location " << full_file_path << std::endl;
                }
                else
                    send_format1(socket, version, 1003);
            }
            else {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "Error in request. Invalid opcode\n";
            }
        }
        return;
    }
    catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "Error in client handling function: " << e.what() << std::endl;
    }

    socket.close();   //Explicitly close socket for this client to ensure proper resource management
}

int main() {   // Main function- the server will create a new thread for every client, and let it run to completion. The server wiats indefinitely for new connections from clients, but manages resources well.
    try {
        boost::asio::io_context io;
        tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 2222));
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "Server running on port 2222...\n";
        }

        while (true) {
            tcp::socket socket(io);
            boost::system::error_code err;
            try {
                acceptor.accept(socket, err);
            }
            catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error accepting connection: " << e.what() << std::endl;
            }

            if (err) {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cerr << "Error accepting connection: " << err.message() << std::endl;
                continue;
            }

            std::thread client_thread(handle_client, std::move(socket));
            client_thread.detach(); // Let it run independently
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error in server: " << e.what() << std::endl;
    }
}