
#ifndef LOOM_CONTROL_H
#define LOOM_CONTROL_H

/**
 * Initialize and start the control server
 * 
 * @param port  The port to listen on
 * @return 0 on success, -1 on failure
 */
int control_server_init(int port);

#endif /* LOOM_CONTROL_H */