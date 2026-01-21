A MQTT client that updates a HOME structure based on MQTT messages.

The home structure is defined as follows:

```go
type Home struct {
    Sensor map[string]*Sensor
}

type SensorType int
const (
    SensorTypeTemperature SensorType = iota
    SensorTypeHumidity
    SensorTypeLight
)

type Sensor struct {
    URL         string      // e.g., "living_room/temperature", unique identifier
    Descr       string      // e.g., "Living Room Temperature"
    Type        SensorType  // e.g., SensorTypeTemperature
    Value       float64     // e.g., 23.5
    Timestamp   int64       // Unix timestamp of the last update
}
```

The MQTT client should subscribe to topics in the following format:

`home/{room_name}/{sensor_name}/sensor`

For each topic the parsing of the payload can be very different, so we need to be able to specify a custom parser function for each topic. The parser function should take the message payload as input and return one or more Sensor structures.

The Application also has an automation engine that runs on a go-routine. The automation engine polls a channel receiving pointers to Sensor structures. The pointer to a Sensor structure received should be used to evaluate automation rules. The automation engine should NOT update any data in the Sensor structure, it should be treated as read-only. When the automation engine has finished evaluating related rules, it should push the Sensor structure pointer to a channel giving it back to the MQTT engine. The Automation engine can copy/store Sensor structures in a map to be able to identify what has changed since the last evaluation.

The MQTT client should push any updated Sensor to the automation engine channel whenever a Sensor is updated. This does mean that the automation engine may receive many updates in a short time frame if many MQTT messages are received quickly. The automation engine should be able to handle this.

Furthermore, the Automation engine has a separate Scheduler engine that can schedule actions to be executed at a later time. The Scheduler engine should be able to schedule actions based on time (e.g., execute action at specific time) or based on Sensor state changes (e.g., execute action when Sensor value crosses a threshold). The Scheduler engine should run in its own go-routine but should not execute the actions itself. Instead, it should push the scheduled actions to a channel that the Automation engine listens to. The Automation engine should then execute the actions when they are received from the Scheduler engine.

