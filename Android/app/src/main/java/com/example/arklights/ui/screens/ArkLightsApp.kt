package com.example.arklights.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.example.arklights.api.ArkLightsApiService
import com.example.arklights.bluetooth.BluetoothService
import com.example.arklights.data.ConnectionState
import com.example.arklights.viewmodel.ArkLightsViewModel
import kotlinx.coroutines.launch

@Composable
fun ArkLightsApp(
    bluetoothService: BluetoothService,
    apiService: ArkLightsApiService,
    modifier: Modifier = Modifier,
    viewModel: ArkLightsViewModel = viewModel {
        ArkLightsViewModel(bluetoothService, apiService)
    }
) {
    val connectionState by viewModel.connectionState.collectAsState()
    val currentPage by viewModel.currentPage.collectAsState()
    val deviceStatus by viewModel.deviceStatus.collectAsState()
    val scope = rememberCoroutineScope()
    
    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Header
        Text(
            text = "ArkLights PEV Control",
            style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.primary
        )
        
        Spacer(modifier = Modifier.height(8.dp))
        
        // Connection Status
        ConnectionStatusCard(
            connectionState = connectionState,
            deviceStatus = deviceStatus
        )
        
        Spacer(modifier = Modifier.height(16.dp))
        
        when (connectionState) {
            ConnectionState.DISCONNECTED -> {
                DeviceDiscoveryScreen(viewModel = viewModel)
            }
            ConnectionState.CONNECTING -> {
                ConnectingScreen()
            }
            ConnectionState.CONNECTED -> {
                MainControlScreen(
                    currentPage = currentPage,
                    onPageChange = viewModel::setCurrentPage,
                    viewModel = viewModel
                )
            }
            ConnectionState.ERROR -> {
            ErrorScreen(
                onRetry = { 
                    scope.launch {
                        viewModel.disconnect()
                    }
                },
                viewModel = viewModel
            )
            }
        }
    }
}

@Composable
fun ConnectionStatusCard(
    connectionState: ConnectionState,
    deviceStatus: com.example.arklights.data.LEDStatus?
) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(
            containerColor = when (connectionState) {
                ConnectionState.CONNECTED -> MaterialTheme.colorScheme.primaryContainer
                ConnectionState.CONNECTING -> MaterialTheme.colorScheme.secondaryContainer
                ConnectionState.ERROR -> MaterialTheme.colorScheme.errorContainer
                else -> MaterialTheme.colorScheme.surfaceVariant
            }
        )
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                Text(
                    text = "Status: ${connectionState.name}",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold
                )
                
                if (connectionState == ConnectionState.CONNECTED) {
                    Text(
                        text = "v8.0 OTA",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
            
            if (deviceStatus != null && connectionState == ConnectionState.CONNECTED) {
                Spacer(modifier = Modifier.height(8.dp))
                
                Text(
                    text = "Build Date: ${deviceStatus.build_date}",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                
                Text(
                    text = "WiFi AP: ${deviceStatus.apName}",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }
    }
}

@Composable
fun ConnectingScreen() {
    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            CircularProgressIndicator()
            Spacer(modifier = Modifier.height(16.dp))
            Text(
                text = "Connecting to ArkLights device...",
                style = MaterialTheme.typography.bodyLarge
            )
        }
    }
}

@Composable
fun ErrorScreen(
    onRetry: () -> Unit,
    viewModel: ArkLightsViewModel
) {
    val errorMessage by viewModel.errorMessage.collectAsState(initial = null)
    
    Card(
        modifier = Modifier.fillMaxWidth()
    ) {
        Column(
            modifier = Modifier.padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                text = "Connection Error",
                style = MaterialTheme.typography.headlineSmall,
                color = MaterialTheme.colorScheme.error
            )
            
            Spacer(modifier = Modifier.height(8.dp))
            
            val errorText = errorMessage
            if (errorText != null) {
                Text(
                    text = errorText,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            
            Spacer(modifier = Modifier.height(16.dp))
            
            Button(
                onClick = onRetry,
                colors = ButtonDefaults.buttonColors(
                    containerColor = MaterialTheme.colorScheme.error
                )
            ) {
                Text("Retry Connection")
            }
        }
    }
}
