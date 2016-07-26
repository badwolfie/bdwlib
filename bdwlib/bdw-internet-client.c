/* bdw-internet-client.c
 *
 * Copyright (C) 2016 Ian Hernandez <ihernandezs@openmailbox.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <unistd.h>

#include "bdw-internet-client.h"
#include "bdw-utils.h"

BdwInternetClient *
bdw_internet_client_new (conststring host, uint16 port, BdwSocketType type)
{
  BdwInternetClient * self = bdw_new (BdwInternetClient);
  bdw_utils_clear_buffer (&self->host, sizeof (self->host));
  self->socket_type = type;

  self->host.sin_family = BDW_INTERNET_DOMAIN;
  self->host.sin_port = htons (port);

  bdw_internet_client_set_host (self, host);

  self->socket_id = socket (BDW_INTERNET_DOMAIN, self->socket_type, 0);
  bdw_error_on_code (self->socket_id, BDW_INTERNET_ERROR_KO, "socket()");

  return self;
}

void
bdw_internet_client_destroy (BdwInternetClient * self)
{
  bdw_free (self);
}

void
bdw_internet_client_set_host (BdwInternetClient * self, conststring host)
{
  conststring real_host;
  BdwError * inner_error = NULL;
  bool host_is_ip = bdw_internet_str_is_ip (host, &inner_error);
  if (inner_error != NULL) {
    bdw_error_report ("%s\n", inner_error->message);
    bdw_error_destroy (inner_error);
    exit (1);
  }

  if (!host_is_ip) {
    real_host = bdw_internet_get_hostname_ip (host, &inner_error);
    if (inner_error != NULL) {
      bdw_error_report ("%s\n", inner_error->message);
      bdw_error_destroy (inner_error);
      exit (1);
    }
  } else {
    real_host = host;
  }

#ifdef USING_IPv6
#error "IPv6 not fully supported yet."
#else
  self->host.sin_addr.s_addr = inet_addr (real_host);
#endif
}

BdwInternetError
bdw_internet_client_connect (const BdwInternetClient * self, uint8 tries)
{
  if (self->socket_type == BDW_SOCKET_TYPE_UDP) {
    return BDW_INTERNET_ERROR_OK;
  }

  int8 try_number = 0;
  int64 status = BDW_INTERNET_ERROR_OK;

  while (try_number < bdw_internet_truncate_connection_tries (tries)) {
    status = connect (self->socket_id, (struct sockaddr *) &self->host,
                      sizeof (self->host));
    try_number++;
  }

  if (status == BDW_INTERNET_ERROR_KO) {
    switch (errno) {
      case EISCONN:
        return BDW_INTERNET_ERROR_ALREADY_CONNECTED;

      case EBADF:
        return BDW_INTERNET_ERROR_INVALID_SOCKET;

      case ENOTSOCK:
        return BDW_INTERNET_ERROR_NOT_A_SOCKET;

      case EADDRNOTAVAIL:
        return BDW_INTERNET_ERROR_ADDRESS_NOT_AVAILABLE;

      case ECONNREFUSED:
        return BDW_INTERNET_ERROR_CONNECTION_REFUSED;

      case ENETUNREACH:
        return BDW_INTERNET_ERROR_NETWORK_UNREACHABLE;

      case EPROTOTYPE:
        return BDW_INTERNET_ERROR_NOT_SUPPORTED;

      case ETIMEDOUT:
        return BDW_INTERNET_ERROR_TIMEOUT;

      default:
        return BDW_INTERNET_ERROR_UNDEFINED;
    }
  }

  return BDW_INTERNET_ERROR_OK;
}

BdwInternetError
bdw_internet_client_send (const BdwInternetClient * self, constpointer buffer,
                          sizetype buffer_length)
{
  int64 sent_bytes;
  sizetype server_size = sizeof (self->host);

  switch (self->socket_type) {
    case BDW_SOCKET_TYPE_TCP:
      sent_bytes = send (self->socket_id, buffer, buffer_length, 0);
      break;

    case BDW_SOCKET_TYPE_UDP:
      sent_bytes =
          sendto (self->socket_id, buffer, buffer_length, 0,
                  (struct sockaddr *) &self->host, (socklen_t) server_size);
      break;

    default:
      sent_bytes = BDW_INTERNET_ERROR_KO;
      break;
  }

  if (sent_bytes == BDW_INTERNET_ERROR_KO) {
    switch (errno) {
      case EINVAL:
        return BDW_INTERNET_ERROR_INVALID_ARGUMENT;

      case ENOTSOCK:
        return BDW_INTERNET_ERROR_NOT_A_SOCKET;

      case ENOTCONN:
        return BDW_INTERNET_ERROR_SOCKET_NOT_CONNECTED;

      case ECONNRESET:
        return BDW_INTERNET_ERROR_CONNECTION_RESET;

      case EOPNOTSUPP:
        return BDW_INTERNET_ERROR_NOT_SUPPORTED;

      case ENOMEM:
        return BDW_INTERNET_ERROR_NO_MEMORY;

      case EBADF:
        return BDW_INTERNET_ERROR_INVALID_SOCKET;

      default:
        return BDW_INTERNET_ERROR_UNDEFINED;
    }
  }

  return BDW_INTERNET_ERROR_OK;
}

BdwInternetError
bdw_internet_client_receive (const BdwInternetClient * self, pointer buffer,
                             sizetype buffer_length)
{
  int64 recv_bytes;
  sizetype server_size = sizeof (self->host);

  switch (self->socket_type) {
    case BDW_SOCKET_TYPE_TCP:
      recv_bytes = recv (self->socket_id, buffer, buffer_length, 0);
      break;

    case BDW_SOCKET_TYPE_UDP:
      recv_bytes = recvfrom (self->socket_id, buffer, buffer_length, 0,
                             (struct sockaddr *) &self->host,
                             (socklen_t *) &server_size);
      break;

    default:
      recv_bytes = BDW_INTERNET_ERROR_KO;
      break;
  }

  if (recv_bytes == BDW_INTERNET_ERROR_KO) {
    switch (errno) {
      case EINVAL:
        return BDW_INTERNET_ERROR_INVALID_ARGUMENT;

      case ECONNREFUSED:
        return BDW_INTERNET_ERROR_CONNECTION_REFUSED;

      case EFAULT:
        return BDW_INTERNET_ERROR_FAULTY_BUFFER;

      case EBADF:
        return BDW_INTERNET_ERROR_INVALID_SOCKET;

      case ENOTCONN:
        return BDW_INTERNET_ERROR_SOCKET_NOT_CONNECTED;

      case ENOMEM:
        return BDW_INTERNET_ERROR_NO_MEMORY;

      case ENOTSOCK:
        return BDW_INTERNET_ERROR_NOT_A_SOCKET;

      default:
        return BDW_INTERNET_ERROR_UNDEFINED;
    }
  }

  return BDW_INTERNET_ERROR_OK;
}

void
bdw_internet_client_shutdown (BdwInternetClient * self)
{
  bdw_utils_clear_buffer (&self->host, sizeof (self->host));
  close (self->socket_id);
}
