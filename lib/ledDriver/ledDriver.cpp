#include <ledDriver.h>

LEDDriver::LEDDriver(const std::map<ColorChannel, int> &pinMap, int freq, int resolution) :
    _pinMap(pinMap), _freq(freq), _resolution(resolution), _channelCounter(0)
{
    _pinMap = pinMap;
    _freq = freq;
    _resolution = resolution;
    _maxDuty = (1 << _resolution) - 1;

    // iterate throught map
    for (const auto &pair : _pinMap)
    {
        ColorChannel channelName = pair.first;
        int pin = pair.second;

        // assign unique led channel to each pin
        int newChannel = _channelCounter++;
        _channelMap[channelName] = newChannel;

        // configure and attach pin
        // pinMode(pin, OUTPUT);
        ledcSetup(newChannel, _freq, _resolution);
        ledcAttachPin(pin, newChannel);
    }
}

void LEDDriver::setColor(ColorChannel channel, uint32_t colorValue)
{
    if (_channelMap.count(channel))
    {
        int ledcChannel = _channelMap[channel];
        // constrain brightness to valid range
        uint32_t clampedBrightness = constrain(colorValue, 0, _maxDuty);
        ledcWrite(ledcChannel, clampedBrightness);
        _currentColor[channel] = clampedBrightness;
    }
}

void LEDDriver::setAllColor(ColorChannel channel, uint32_t colorValue)
{
    for (const auto &pair : _pinMap)
    {
        setColor(pair.first, colorValue);
    }
}

void LEDDriver::setColor(const uint8_t *colorsArray)
{
    setColor(RED, map(colorsArray[0], 0, 255, 0, _maxDuty));
    setColor(GRN, map(colorsArray[1], 0, 255, 0, _maxDuty));
    setColor(BLU, map(colorsArray[2], 0, 255, 0, _maxDuty));
    setColor(WW, map(colorsArray[3], 0, 255, 0, _maxDuty));
    setColor(CW, map(colorsArray[4], 0, 255, 0, _maxDuty));
}

uint32_t LEDDriver::getColor(ColorChannel channel) const
{
    if (_currentColor.count(channel))
    {
        return _currentColor.at(channel);
    }
    return 0;
}
