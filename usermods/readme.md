# Usermods

¡Esta carpeta sirve como repositorio para usermods (archivos personalizados `usermod.cpp`)!

Si ha creado un usermod que cree que es útil (por ejemplo, para soportar un sensor particular, pantalla, característica...), ¡siéntase libre de contribuir abriendo una solicitud de extracción!

Para que otras personas puedan disfrutar de su usermod, tenga en cuenta estos puntos:

* Cree una carpeta en esta carpeta con un nombre descriptivo (por ejemplo `usermod_ds18b20_temp_sensor_mqtt`)  
* Incluya sus archivos personalizados 
* Si su usermod requiere cambios en otros archivos de WLED, escriba un `readme.md` describiendo los pasos que debe seguir  
* ¡Cree una solicitud de extracción!  
* Si su característica es útil para la mayoría de usuarios de WLED, consideraré agregarla al código base!  

Aunque hago mi mejor esfuerzo para no romper demasiado, tenga en cuenta que a medida que se actualiza WLED, los usermods pueden romperse.  
No estoy manteniendo activamente ningún usermod en este directorio, esa es su responsabilidad como creador del usermod.

Para nuevos usermods, recomiendo probar la nueva API de usermod v2, ¡que permite instalar múltiples usermods a la vez y nuevas funciones!
Puede echar un vistazo a `EXAMPLE_v2` para algunas documentaciones y a `Temperature` para un usermod v2 completado.

¡Gracias por tu ayuda :)
