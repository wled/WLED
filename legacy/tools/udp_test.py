import numpy as np
import socket

class WledRealtimeClient:
    def __init__(self, wled_controller_ip, num_pixels, udp_port=21324, max_pixels_per_packet=126):
        self.wled_controller_ip = wled_controller_ip
        self.num_pixels = num_pixels
        self.udp_port = udp_port
        self.max_pixels_per_packet = max_pixels_per_packet
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._prev_pixels = np.full((3, self.num_pixels), 253, dtype=np.uint8)
        self.pixels = np.full((3, self.num_pixels), 1, dtype=np.uint8)
    
    def update(self):
        # Truncate values and cast to integer
        self.pixels = np.clip(self.pixels, 0, 255).astype(np.uint8)
        p = np.copy(self.pixels)
        
        idx = np.where(~np.all(p == self._prev_pixels, axis=0))[0]
        num_pixels = len(idx)
        n_packets = (num_pixels + self.max_pixels_per_packet - 1) // self.max_pixels_per_packet
        idx_split = np.array_split(idx, n_packets)
        
        header = bytes([1, 2])  # WARLS protocol header
        for packet_indices in idx_split:
            data = bytearray(header)
            for i in packet_indices:
                data.extend([i, *p[:, i]])  # Index and RGB values
            self._sock.sendto(bytes(data), (self.wled_controller_ip, self.udp_port))
        
        self._prev_pixels = np.copy(p)



################################## LED blink test ##################################
if __name__ == "__main__":
    WLED_CONTROLLER_IP = "192.168.1.153"
    NUM_PIXELS = 255 # Amount of LEDs on your strip
    import time
    wled = WledRealtimeClient(WLED_CONTROLLER_IP, NUM_PIXELS)
    print('Starting LED blink test')
    while True:
        for i in range(NUM_PIXELS):
            wled.pixels[1, i] = 255 if wled.pixels[1, i] == 0 else 0
        wled.update()
        time.sleep(.01)
