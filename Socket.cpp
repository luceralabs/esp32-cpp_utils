/*
 * Socket.cpp
 *
 *  Created on: Mar 5, 2017
 *      Author: kolban
 */
#include <iostream>
#include <streambuf>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <string>

#include <sstream>

#include <errno.h>
#include <esp_log.h>
#include <lwip/sockets.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include "GeneralUtils.h"
#include "sdkconfig.h"
#include "Socket.h"

static const char* LOG_TAG = "Socket";

Socket::Socket() {
	m_sock = -1;
}

Socket::~Socket() {
	//close_cpp(); // When the class instance has ended, delete the socket.
}



Socket Socket::accept_cpp() {
	struct sockaddr addr;
	getBind_cpp(&addr);
	ESP_LOGD(LOG_TAG, "Accepting on %s", addressToString(&addr).c_str());
	struct sockaddr_in clientAddress;
	socklen_t clientAddressLength = sizeof(clientAddress);
	int clientSockFD = ::accept(m_sock, (struct sockaddr *)&clientAddress, &clientAddressLength);
	if (clientSockFD == -1) {
		ESP_LOGE(LOG_TAG, "accept(): %s, m_sock=%d", strerror(errno), m_sock);
		Socket newSocket;
		return newSocket;
	}
	Socket newSocket;
	newSocket.m_sock = clientSockFD;
	return newSocket;
}

/**
 * @brief Convert a socket address to a string representation.
 * @param [in] addr The address to parse.
 * @return A string representation of the address.
 */
std::string Socket::addressToString(struct sockaddr* addr) {
	struct sockaddr_in *pInAddr = (struct sockaddr_in *)addr;
	char temp[30];
	char ip[20];
	inet_ntop(AF_INET, &pInAddr->sin_addr, ip, sizeof(ip));
	sprintf(temp, "%s [%d]", ip, ntohs(pInAddr->sin_port));
	return std::string(temp);
} // addressToString


/**
 * @brief Bind an address/port to a socket.
 * Specify a port of 0 to have a local port allocated.
 * Specify an address of INADDR_ANY to use the local server IP.
 * @param [in] port Port number to bind.
 * @param [in] address Address to bind.
 * @return N/A
 */
void Socket::bind_cpp(uint16_t port, uint32_t address) {
	ESP_LOGD(LOG_TAG, "bind_cpp: port=%d, address=0x%x", port, address);
	if (m_sock == -1) {
		ESP_LOGE(LOG_TAG, "bind_cpp: Socket is not initialized.");
	}
	struct sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(address);
	serverAddress.sin_port   = htons(port);
	int rc = ::bind(m_sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
	if (rc == -1) {
		ESP_LOGE(LOG_TAG, "bind_cpp: bind[socket=%d]: %d: %s", m_sock, errno, strerror(errno));
		return;
	}
} // bind_cpp


/**
 * @brief Close the socket.
 *
 * @return N/A.
 */
void Socket::close_cpp() {
	ESP_LOGD(LOG_TAG, "close_cpp: m_sock=%d", m_sock);
	if (m_sock != -1) {
		::close(m_sock);
	}
	m_sock = -1;
} // close_cpp


/**
 * @brief Connect to a partner.
 *
 * @param [in] address The IP address of the partner.
 * @param [in] port The port number of the partner.
 * @return Success or failure of the connection.
 */
int Socket::connect_cpp(struct in_addr address, uint16_t port) {
	struct sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr   = address;
	serverAddress.sin_port   = htons(port);
	char msg[50];
	inet_ntop(AF_INET, &address, msg, sizeof(msg));
	ESP_LOGD(LOG_TAG, "Connecting to %s:[%d]", msg, port);
	createSocket_cpp();
	int rc = ::connect(m_sock, (struct sockaddr *)&serverAddress, sizeof(struct sockaddr_in));
	if (rc == -1) {
		ESP_LOGE(LOG_TAG, "connect_cpp: Error: %s", strerror(errno));
		close_cpp();
		return -1;
	} else {
		ESP_LOGD(LOG_TAG, "Connected to partner");
		return 0;
	}
} // connect_cpp


/**
 * @brief Connect to a partner.
 *
 * @param [in] strAddress The string representation of the IP address of the partner.
 * @param [in] port The port number of the partner.
 * @return Success or failure of the connection.
 */
int Socket::connect_cpp(char* strAddress, uint16_t port) {
	struct in_addr address;
	inet_pton(AF_INET, (char *)strAddress, &address);
	return connect_cpp(address, port);
}


/**
 * @brief Create the socket.
 * @param [in] isDatagram Set to true to create a datagram socket.  Default is false.
 * @return The socket descriptor.
 */
int Socket::createSocket_cpp(bool isDatagram) {
	if (isDatagram) {
		m_sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	}
	else {
		m_sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	}
	if (m_sock == -1) {
		ESP_LOGE(LOG_TAG, "createSocket_cpp: socket: %d", errno);
		return m_sock;
	}
	return m_sock;
} // createSocket_cpp


/**
 * @brief Get the bound address.
 * @param [out] pAddr The storage to hold the address.
 * @return N/A.
 */
void Socket::getBind_cpp(struct sockaddr* pAddr) {
	if (m_sock == -1) {
		ESP_LOGE(LOG_TAG, "getBind_cpp: Socket is not initialized.");
	}
	socklen_t nameLen = sizeof(struct sockaddr);
	::getsockname(m_sock, pAddr, &nameLen);
} // getBind_cpp


/**
 * @brief Get the underlying socket file descriptor.
 * @return The underlying socket file descriptor.
 */
int Socket::getFD() const {
	return m_sock;
} // getFD


bool Socket::isValid() {
	return m_sock != -1;
} // isValid

/**
 * @brief Create a listening socket.
 * @param [in] port The port number to listen upon.
 * @param [in] isDatagram True if we are listening on a datagram.
 */
void Socket::listen_cpp(uint16_t port, bool isDatagram) {
	createSocket_cpp(isDatagram);
	bind_cpp(port, 0);
	int rc = ::listen(m_sock, 5);
	if (rc == -1) {
		ESP_LOGE(LOG_TAG, "listen_cpp: %s", strerror(errno));
	}
} // listen_cpp

bool Socket::operator <(const Socket& other) const {
	return m_sock < other.m_sock;
}


std::string Socket::readToDelim(std::string delim) {
	std::string ret;
	std::string part;
	auto it = delim.begin();
	while(1) {
		uint8_t val;
		int rc = receive_cpp(&val, 1);
		if (rc == -1) {
			return "";
		}
		if (rc == 0) {
			return ret+part;
		}
		if (*it == val) {
			part+= val;
			++it;
			if (it == delim.end()) {
				return ret;
			}
		} else {
			if (part.empty()) {
				ret += part;
				part.clear();
				it = delim.begin();
			}
			ret += val;
		}
	} // While
} // readToDelim



/**
 * @brief Receive data from the partner.
 * Receive data from the socket partner.  If exact = false, we read as much data as
 * is available without blocking up to length.  If exact = true, we will block until
 * we have received exactly length bytes or there are no more bytes to read.
 * @param [in] data The buffer into which the received data will be stored.
 * @param [in] length The size of the buffer.
 * @param [in] exact Read exactly this amount.
 * @return The length of the data received or -1 on an error.
 */
size_t Socket::receive_cpp(uint8_t* data, size_t length, bool exact) {
	//ESP_LOGD(LOG_TAG, ">> receive_cpp: length: %d, exact: %d", length, exact);
	if (exact == false) {
		int rc = ::recv(m_sock, data, length, 0);
		if (rc == -1) {
			ESP_LOGE(LOG_TAG, "receive_cpp: %s", strerror(errno));
		}
		//GeneralUtils::hexDump(data, rc);
		return rc;
	}
	size_t amountToRead = length;
	while(amountToRead > 0) {
		int rc = ::recv(m_sock, data, amountToRead, 0);
		if (rc == -1) {
			ESP_LOGE(LOG_TAG, "receive_cpp: %s", strerror(errno));
			return 0;
		}
		if (rc == 0) {
			break;
		}
		amountToRead -= rc;
		data+= rc;
	}
	//GeneralUtils::hexDump(data, length);
	return length;
} // receive_cpp


/**
 * @brief Receive data with the address.
 * @param [in] data The location where to store the data.
 * @param [in] length The size of the data buffer into which we can receive data.
 * @param [in] pAddr An area into which we can store the address of the partner.
 * @return The length of the data received.
 */
int Socket::receiveFrom_cpp(uint8_t* data, size_t length,	struct sockaddr *pAddr) {
	socklen_t addrLen = sizeof(struct sockaddr);
	int rc = ::recvfrom(m_sock, data, length, 0, pAddr, &addrLen);
	return rc;
} // receiveFrom_cpp


/**
 * @brief Send data to the partner.
 *
 * @param [in] data The buffer containing the data to send.
 * @param [in] length The length of data to be sent.
 * @return N/A.
 *
 */
int Socket::send_cpp(const uint8_t* data, size_t length) const {
	ESP_LOGD(LOG_TAG, "send_cpp: Raw binary of length: %d", length);
	//GeneralUtils::hexDump(data, length);
	int rc = ::send(m_sock, data, length, 0);
	if (rc == -1) {
		ESP_LOGE(LOG_TAG, "send: socket=%d, %s", m_sock, strerror(errno));
	}
	return rc;
} // send_cpp


/**
 * @brief Send a string to the partner.
 *
 * @param [in] value The string to send to the partner.
 * @return N/A.
 */
int Socket::send_cpp(std::string value) const {
	ESP_LOGD(LOG_TAG, "send_cpp: Binary of length: %d", value.length());
	return send_cpp((uint8_t *)value.data(), value.size());
} // send_cpp


int Socket::send_cpp(uint16_t value) {
	ESP_LOGD(LOG_TAG, "send_cpp: 16bit value: %.2x", value);
	return send_cpp((uint8_t *)&value, sizeof(value));
} // send_cpp

int  Socket::send_cpp(uint32_t value) {
	ESP_LOGD(LOG_TAG, "send_cpp: 32bit value: %.2x", value);
	return send_cpp((uint8_t *)&value, sizeof(value));
} // send_cpp


/**
 * @brief Send data to a specific address.
 * @param [in] data The data to send.
 * @param [in] length The length of the data to send/
 * @param [in] pAddr The address to send the data.
 */
void Socket::sendTo_cpp(const uint8_t* data, size_t length, struct sockaddr* pAddr) {
	int rc = ::sendto(m_sock, data, length, 0, pAddr, sizeof(struct sockaddr));
	if (rc == -1) {
		ESP_LOGE(LOG_TAG, "sendto_cpp: socket=%d %s", m_sock, strerror(errno));
	}
} // sendTo_cpp

/**
 * @brief Get the string representation of this socket
 * @return the string representation of the socket.
 */
std::string Socket::toString() {
	std::ostringstream oss;
	oss << "fd: " << m_sock;
	return oss.str();
} // toString


/**
 * @brief Create a socket input record streambuf
 * @param [in] socket The socket we will be reading from.
 * @param [in] dataLength The size of a record.
 * @param [in] bufferSize The size of the buffer we wish to allocate to hold data.
 */
SocketInputRecordStreambuf::SocketInputRecordStreambuf(
	Socket  socket,
	size_t  dataLength,
	size_t  bufferSize) {
	m_socket     = socket;    // The socket we will be reading from
	m_dataLength = dataLength; // The size of the record we wish to read.
	m_bufferSize = bufferSize; // The size of the buffer used to hold data
	m_sizeRead   = 0;          // The size of data read from the socket
	m_buffer = new char[bufferSize]; // Create the buffer used to hold the data read from the socket.

	setg(m_buffer, m_buffer, m_buffer); // Set the initial get buffer pointers to no data.
} // SocketInputRecordStreambuf

SocketInputRecordStreambuf::~SocketInputRecordStreambuf() {
	delete[] m_buffer;
} // ~SocketInputRecordStreambuf


/**
 * @brief Handle the request to read data from the stream but we need more data from the source.
 *
 */
SocketInputRecordStreambuf::int_type SocketInputRecordStreambuf::underflow() {
	if (m_sizeRead >= m_dataLength) {
		return EOF;
	}
	int bytesRead = m_socket.receive_cpp((uint8_t*)m_buffer, m_bufferSize, true);
	if (bytesRead == 0) {
		return EOF;
	}
	m_sizeRead += bytesRead;
	setg(m_buffer, m_buffer, m_buffer + bytesRead);
	return traits_type::to_int_type(*gptr());
} // underflow
