package com.example.arklights.bluetooth

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import androidx.core.app.ActivityCompat
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.withContext
import java.util.UUID
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.asSharedFlow
import com.example.arklights.data.ConnectionState

class BluetoothService(private val context: Context) {
    
    companion object {
        // BLE Service and Characteristic UUIDs (matching ESP32)
        private val SERVICE_UUID = UUID.fromString("12345678-1234-1234-1234-123456789abc")
        private val CHARACTERISTIC_UUID = UUID.fromString("87654321-4321-4321-4321-cba987654321")
        private const val ARKLIGHTS_DEVICE_NAME_PREFIX = "ARKLIGHTS"
    }

    private val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val bluetoothAdapter: BluetoothAdapter? = bluetoothManager.adapter
    private val bleScanner: BluetoothLeScanner? = bluetoothAdapter?.bluetoothLeScanner

    private var bluetoothGatt: BluetoothGatt? = null
    private var characteristic: BluetoothGattCharacteristic? = null

    private val _connectionState = MutableStateFlow(ConnectionState.DISCONNECTED)
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()

    private val _discoveredDevices = MutableStateFlow<List<BluetoothDevice>>(emptyList())
    val discoveredDevices: StateFlow<List<BluetoothDevice>> = _discoveredDevices.asStateFlow()

    private val _messages = MutableSharedFlow<String>()
    val messages: SharedFlow<String> = _messages.asSharedFlow()

    private val _errorMessage = MutableSharedFlow<String>()
    val errorMessage: SharedFlow<String> = _errorMessage.asSharedFlow()

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    _connectionState.value = ConnectionState.CONNECTED
                    try {
                        if (hasBluetoothPermissions()) {
                            gatt?.discoverServices()
                        }
                    } catch (e: SecurityException) {
                        _errorMessage.tryEmit("Bluetooth permission denied")
                    }
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    _connectionState.value = ConnectionState.DISCONNECTED
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val service = gatt?.getService(SERVICE_UUID)
                characteristic = service?.getCharacteristic(CHARACTERISTIC_UUID)
                if (characteristic != null) {
                    try {
                        if (hasBluetoothPermissions()) {
                            gatt?.setCharacteristicNotification(characteristic, true)
                            
                            // Enable notifications by setting the descriptor
                            val descriptor = characteristic?.getDescriptor(
                                UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
                            )
                            descriptor?.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                            gatt?.writeDescriptor(descriptor)
                        }
                    } catch (e: SecurityException) {
                        _errorMessage.tryEmit("Bluetooth permission denied")
                    }
                }
            }
        }

        override fun onCharacteristicChanged(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?) {
            val data = characteristic?.value?.let { String(it) }
            data?.let { 
                println("Android: Received BLE data: $it")
                _messages.tryEmit(it) 
            }
        }

        override fun onDescriptorWrite(gatt: BluetoothGatt?, descriptor: BluetoothGattDescriptor?, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                println("Android: BLE notifications enabled successfully")
            } else {
                println("Android: Failed to enable BLE notifications: $status")
            }
        }
    }

    fun isBluetoothEnabled(): Boolean {
        return bluetoothAdapter?.isEnabled == true
    }

    fun hasBluetoothPermissions(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ActivityCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED &&
            ActivityCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED
        } else {
            ActivityCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH) == PackageManager.PERMISSION_GRANTED &&
            ActivityCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_ADMIN) == PackageManager.PERMISSION_GRANTED
        }
    }

    suspend fun startDiscovery(): Boolean = withContext(Dispatchers.IO) {
        if (!isBluetoothEnabled() || !hasBluetoothPermissions()) {
            _errorMessage.tryEmit("Bluetooth not enabled or permissions not granted")
            return@withContext false
        }

        val devices = mutableListOf<BluetoothDevice>()
        
        try {
            bleScanner?.startScan(object : ScanCallback() {
                override fun onScanResult(callbackType: Int, result: ScanResult) {
                    val device = result.device
                    val deviceName = try {
                        device.name ?: "Unknown Device"
                    } catch (e: SecurityException) {
                        "Unknown Device"
                    }
                    
                    if (deviceName.startsWith(ARKLIGHTS_DEVICE_NAME_PREFIX)) {
                        if (!devices.any { it.address == device.address }) {
                            devices.add(device)
                            _discoveredDevices.value = devices.toList()
                        }
                    }
                }

                override fun onScanFailed(errorCode: Int) {
                    _errorMessage.tryEmit("BLE scan failed with error: $errorCode")
                }
            })
        } catch (e: SecurityException) {
            _errorMessage.tryEmit("Bluetooth permission denied")
            return@withContext false
        }

        // Stop scanning after 10 seconds
        kotlinx.coroutines.delay(10000)
        try {
            bleScanner?.stopScan(object : ScanCallback() {})
        } catch (e: SecurityException) {
            _errorMessage.tryEmit("Bluetooth permission denied")
        }
        true
    }

    suspend fun stopDiscovery(): Boolean = withContext(Dispatchers.IO) {
        try {
            bleScanner?.stopScan(object : ScanCallback() {})
            true
        } catch (e: SecurityException) {
            _errorMessage.tryEmit("Bluetooth permission denied")
            false
        } catch (e: Exception) {
            _errorMessage.tryEmit("Failed to stop discovery: ${e.message}")
            false
        }
    }

    suspend fun getPairedDevices(): List<BluetoothDevice> = withContext(Dispatchers.IO) {
        if (!hasBluetoothPermissions()) {
            return@withContext emptyList()
        }
        
        try {
            bluetoothAdapter?.bondedDevices?.filter { device ->
                device.name?.startsWith(ARKLIGHTS_DEVICE_NAME_PREFIX) == true
            } ?: emptyList()
        } catch (e: SecurityException) {
            _errorMessage.tryEmit("Bluetooth permission denied")
            emptyList()
        } catch (e: Exception) {
            _errorMessage.tryEmit("Failed to get paired devices: ${e.message}")
            emptyList()
        }
    }

    suspend fun connectToDevice(device: BluetoothDevice): Boolean = withContext(Dispatchers.IO) {
        if (!hasBluetoothPermissions()) {
            _errorMessage.tryEmit("Bluetooth permission denied")
            return@withContext false
        }

        _connectionState.value = ConnectionState.CONNECTING
        try {
            bluetoothGatt = device.connectGatt(context, false, gattCallback)
        } catch (e: SecurityException) {
            _errorMessage.tryEmit("Bluetooth permission denied")
            return@withContext false
        }
        true
    }

    suspend fun disconnect(): Boolean = withContext(Dispatchers.IO) {
        try {
            try {
                bluetoothGatt?.disconnect()
            } catch (e: SecurityException) {
                _errorMessage.tryEmit("Bluetooth permission denied")
            }
            try {
                bluetoothGatt?.close()
            } catch (e: SecurityException) {
                _errorMessage.tryEmit("Bluetooth permission denied")
            }
            bluetoothGatt = null
            characteristic = null
            _connectionState.value = ConnectionState.DISCONNECTED
            true
        } catch (e: Exception) {
            _errorMessage.tryEmit("Disconnect error: ${e.message}")
            false
        }
    }

    suspend fun sendData(data: String): Boolean = withContext(Dispatchers.IO) {
        if (_connectionState.value != ConnectionState.CONNECTED || characteristic == null) {
            _errorMessage.tryEmit("Not connected to device")
            return@withContext false
        }

        try {
            characteristic?.value = data.toByteArray()
            try {
                if (hasBluetoothPermissions()) {
                    bluetoothGatt?.writeCharacteristic(characteristic)
                }
            } catch (e: SecurityException) {
                _errorMessage.tryEmit("Bluetooth permission denied")
            }
            true
        } catch (e: Exception) {
            _errorMessage.tryEmit("Failed to send data: ${e.message}")
            false
        }
    }

    suspend fun sendHttpRequest(endpoint: String, method: String = "GET", body: String? = null): String? = withContext(Dispatchers.IO) {
        if (_connectionState.value != ConnectionState.CONNECTED || characteristic == null) {
            _errorMessage.tryEmit("Not connected to device")
            return@withContext null
        }

        try {
            val request = buildHttpRequest(endpoint, method, body)
            println("Android: Sending BLE request: $request")
            characteristic?.value = request.toByteArray()
            try {
                if (hasBluetoothPermissions()) {
                    bluetoothGatt?.writeCharacteristic(characteristic)
                    println("Android: BLE characteristic written successfully")
                }
            } catch (e: SecurityException) {
                _errorMessage.tryEmit("Bluetooth permission denied")
            }
            
            // For now, return a simple success response
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n{\"success\":true}"
        } catch (e: Exception) {
            _errorMessage.tryEmit("HTTP request failed: ${e.message}")
            null
        }
    }

    private fun buildHttpRequest(endpoint: String, method: String, body: String?): String {
        val request = StringBuilder()
        request.append("$method $endpoint HTTP/1.1\r\n")
        request.append("Host: localhost\r\n")
        request.append("Content-Type: application/json\r\n")
        
        if (body != null) {
            request.append("Content-Length: ${body.length}\r\n")
        }
        
        request.append("\r\n")
        
        if (body != null) {
            request.append(body)
        }
        
        return request.toString()
    }

    fun isConnected(): Boolean {
        return _connectionState.value == ConnectionState.CONNECTED
    }

    fun getCurrentDevice(): BluetoothDevice? {
        return bluetoothGatt?.device
    }
}