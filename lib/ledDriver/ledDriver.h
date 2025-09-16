#ifndef __LEDDRIVER_H__
#define __LEDDRIVER_H__

/*** INCLUDES ***/
#include <Arduino.h>
#include <map>

/*** DEFINES ***/
const int DEFAULT_FREQ = 5000;
const int DEFAULT_RESOLUTION = 8;
/*** TYPEDEFS ***/

/*** GLOBAL VARIABLES ***/

/*** FUNCTION PROTOTYPES ***/
bool applyLEDColor(uint8_t *colors);
bool initLEDs(void);

/*** CLASSES  ***/
class LEDDriver
{
public:
    enum ColorChannel
    {
        RED,
        GRN,
        BLU,
        WW,
        CW
    };

    LEDDriver(const std::map<ColorChannel, int>& pinMap, int freq = DEFAULT_FREQ, int resolution = DEFAULT_RESOLUTION);
    void setColor(ColorChannel channel, uint32_t colorValue);
    void setAllColor(ColorChannel channel, uint32_t colorValue);
    void setColor(const uint8_t* colorsArray);
    uint32_t getColor(ColorChannel channel) const;
    
private:
    std::map<ColorChannel, int> _pinMap;
    std::map<ColorChannel, int> _channelMap;
    std::map<ColorChannel, int> _currentColor;
    int _freq;
    int _resolution;
    int _channelCounter;
    int _maxDuty;
};

#endif