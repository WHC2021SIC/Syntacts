from ctypes import *

# DLL Import
_tact = cdll.LoadLibrary("./syntacts-c.dll")

###############################################################################
## SESSION
###############################################################################

class Session:
    def __init__(self):
        self._handle = _tact.Session_create()
    
    def __del__(self):
        _tact.Session_delete(self._handle)

    def open(self, index, channelCount, sampleRate):
        return _tact.Session_open3(self._handle, index, channelCount, sampleRate)

    def close(self):
        return _tact.Session_close(self._handle)
    
    def is_open(self):
        return _tact.Session_isOpen(self._handle)

    def play(self, channel, signal):
        return _tact.Session_play(self._handle, channel, signal._handle)

    def playAll(self, signal):
        return _tact.Session_playAll(self._handle, signal._handle)

    def stop(self, channel):
        return _tact.Session_stop(self._handle, channel)

    def stopAll(self, signal):
        return _tact.Session_stopAll(self._handle)

    def pause(self, channel):
        return _tact.Session_pause(self._handle, channel)

    def pauseAll(self, signal):
        return _tact.Session_pauseAll(self._handle)   

    def resume(self, channel):
        return _tact.Session_resume(self._handle, channel)

    def resumeAll(self, signal):
        return _tact.Session_resumeAll(self._handle)

    def isPlaying(self, channel):
        return _tact.Session_isPlaying(self._handle, channel)

    def isPause(self, channel):
        return _tact.Session_isPaused(self._handle, channel)

    def setVolume(self, channel, volume):
        return _tact.Session_setVolume(self._handle, channel, volume)

    def getVolume(self, channel):
        return _tact.Session_getVolume(self._handle, channel)

    def setPitch(self, channel, pitch):
        return _tact.Session_setPitch(self._handle, channel, pitch)

    def getPitch(self, channel):
        return _tact.Session_getPitch(self._handle, channel)

    def getChannelCount(self):
        return _tact.Session_getChannelCount(self._handle)

    def getSampleRate(self):
        return _tact.Session_getSampleRate(self._handle)

    def getCpuLoad(self):
        return _tact.Session_getCpuLoad(self._handle)

    @staticmethod
    def count():
        return _tact.Session_count()

###############################################################################
## SIGNAL
###############################################################################
class Signal:    
    def __init__(self, handle):
        self._handle = handle

    def __del__(self):
        _tact.Signal_delete(self._handle)

    def sample(self, t):
        return _tact.Signal_sample(self._handle, t)

    def length(self):
        return _tact.Signal_length(self._handle)

    def __mul__(self, other):
        if isinstance(other, Signal):
            return Product(self, other)
        elif isinstance(other, (int, float)):
            return Signal(_tact.Mul_SigFlt(self._handle, other))
        else:
            raise TypeError("other must be Signal, int, or float")

    def __rmul__(self, other):
        if isinstance(other, (int, float)):
            return Signal(_tact.Mul_FltSig(other, self._handle))
        else:
            raise TypeError("other must be int or float")

    def __add__(self, other):
        if isinstance(other, Signal):
            return Sum(self, other)
        elif isinstance(other, (int, float)):
            return Signal(_tact.Add_SigFlt(self._handle, other))
        else:
            raise TypeError("other must be Signal, int, or float")  

    def __radd__(self, other):
        if isinstance(other, (int, float)):
            return Signal(_tact.Add_FltSig(other, self._handle))
        else:
            raise TypeError("other must be int or float")

    def __sub__(self, other):
        if isinstance(other, (int, float)):
            return Signal(_tact.Sub_SigFlt(self._handle, other))
        else:
            raise TypeError("other must be Signal, int, or float")  

    def __rsub__(self, other):
        if isinstance(other, (int, float)):
            return Signal(_tact.Sub_FltSig(other, self._handle))
        else:
            raise TypeError("other must be int or float")         

    def __lshift__(self, other):
        if isinstance(other, Signal):
            return Sequence(_tact.Sequence_SigSig(self._handle, other._handle))
        elif isinstance(other, (int, float)):
            return Sequence(_tact.Sequence_SigFlt(self._handle, other))
        else:
            raise TypeError("other must be Signal, int, or float")

    def __rlshift__(self, other):
        if isinstance(other, (int, float)):
            return Sequence(_tact.Sequence_FltSig(other, self._handle))
        else:
            raise TypeError("other must be int or float")

    @staticmethod
    def count():
        return _tact.Signal_count()

###############################################################################
# OPERATORS
###############################################################################

class Product(Signal):
    def __init__(self, lhs, rhs):
        self._handle = _tact.Product_create(lhs._handle, rhs._handle)

class Sum(Signal):
    def __init__(self, lhs, rhs):
        self._handle = _tact.Sum_create(lhs._handle, rhs._handle)

###############################################################################
# SEQUENCE
###############################################################################

class Sequence(Signal):
    def __init__(self, handle = None):
        if handle:
            self._handle = handle
        else:
            self._handle =  _tact.Sequence_create()
    
    @property
    def head(self):
        return _tact.Sequence_getHead(self._handle)

    @head.setter
    def head(self, newHead):
        _tact.Sequence_setHead(self._handle, newHead)

    def push(self, other):
        if isinstance(other, Sequence):
            _tact.Sequence_SeqSeq(self._handle, other._handle)
        elif isinstance(other, Signal):
            _tact.Sequence_SeqSig(self._handle, other._handle)
        elif isinstance(other, (int, float)):
            _tact.Sequence_SeqFlt(self._handle, other)
        else:
            raise TypeError("other must be Sequence, Signal, int, or float")
        return self

    def insert(self, other, t):
        if isinstance(other, Sequence):
            _tact.Sequence_insertSeq(self._handle, other._handle, t)
        elif isinstance(other, Signal):
            _tact.Sequence_insertSig(self._handle, other._handle, t)
        else:
            raise TypeError("other must be Sequence or Signal")
        return self

    def clear(self):
        _tact.Sequence_clear(self._handle)

    def __lshift__(self, other):
        if isinstance(other, Sequence):
            _tact.Sequence_SeqSeq(self._handle, other._handle)
        elif isinstance(other, Signal):
            _tact.Sequence_SeqSig(self._handle, other._handle)
        elif isinstance(other, (int, float)):
            _tact.Sequence_SeqFlt(self._handle, other)
        else:
            raise TypeError("other must be Sequence, Signal, int, or float")
        return self

###############################################################################
# GENERAL
###############################################################################

class Time(Signal):
    def __init__(self):
        self._handle = _tact.Time_create()

class Scalar(Signal):
    def __init__(self, value):
        self._handle = _tact.Scalar_create(value)

class Ramp(Signal):
    def __init__(self, initial, arg1, arg2=None):
        if arg2:
            self._handle = _tact.Ramp_create2(initial, arg2, arg1)
        else:
            self._handle = _tact.Ramp_create1(initial, arg1)

class Noise(Signal):
    def __init__(self):
        self._handle = _tact.Noise_create()

class Expression(Signal):
    def __init__(self, expr):
        self._handle = _tact.Expression_create(c_char_p(expr.encode))

# TODO: class Samples (not sure how to handle float* yet)

###############################################################################
# PROCESS
###############################################################################

class Repeater(Signal):
    def __init__(self, signal, repetitions, delay):
        self._handle = _tact.Repeater_create(signal._handle, repetitions, delay)

class Stretcher(Signal):
    def __init__(self, signal, factor):
        self._handle = _tact.Stretcher_create(signal._handle, factor)

class Reverser(Signal):
    def __init__(self, signal):
        self._handle = _tact.Reverser_create(signal._handle)

###############################################################################
# ENVELOPE
###############################################################################

class Envelope(Signal):
    def __init__(self, duration, amplitude=1):
        self._handle = _tact.Envelope_create(duration, amplitude)

class ASR(Signal):
    def __init__(self, a, s, r, amplitude=1):
        self._handle = _tact.ASR_create(a,s,r,amplitude)

class ADSR(Signal):
    def __init__(self, a, d, s, r, amp1=1, amp2=0.5):
        self._handle = _tact.ADSR_create(a,d,s,r,amp1,amp2)

###############################################################################
# OSCILLATOR
###############################################################################

class Sine(Signal):
    def __init__(self, arg1, arg2=None, arg3=None):
        if arg3:
            self._handle = _tact.Sine_create4(arg1,arg2._handle,arg3)
        elif arg2:
            self._handle = _tact.Sine_create3(arg1, arg2)
        elif isinstance(arg1, (int, float)): 
            self._handle = _tact.Sine_create2(arg1)
        elif isinstance(arg1, Signal):
            self._handle = _tact.Sine_create1(arg1._handle)
        else:
            raise TypeError("Invalid arguments passed to Sine")

class Square(Signal):
    def __init__(self, arg1, arg2=None, arg3=None):
        if arg3:
            self._handle = _tact.Square_create4(arg1,arg2._handle,arg3)
        elif arg2:
            self._handle = _tact.Square_create3(arg1, arg2)
        elif isinstance(arg1, (int, float)): 
            self._handle = _tact.Square_create2(arg1)
        elif isinstance(arg1, Signal):
            self._handle = _tact.Square_create1(arg1._handle)
        else:
            raise TypeError("Invalid arguments passed to Sine")

class Saw(Signal):
    def __init__(self, arg1, arg2=None, arg3=None):
        if arg3:
            self._handle = _tact.Saw_create4(arg1,arg2._handle,arg3)
        elif arg2:
            self._handle = _tact.Saw_create3(arg1, arg2)
        elif isinstance(arg1, (int, float)): 
            self._handle = _tact.Saw_create2(arg1)
        elif isinstance(arg1, Signal):
            self._handle = _tact.Saw_create1(arg1._handle)
        else:
            raise TypeError("Invalid arguments passed to Sine")

class Triangle(Signal):
    def __init__(self, arg1, arg2=None, arg3=None):
        if arg3:
            self._handle = _tact.Triangle_create4(arg1,arg2._handle,arg3)
        elif arg2:
            self._handle = _tact.Triangle_create3(arg1, arg2)
        elif isinstance(arg1, (int, float)): 
            self._handle = _tact.Triangle_create2(arg1)
        elif isinstance(arg1, Signal):
            self._handle = _tact.Triangle_create1(arg1._handle)
        else:
            raise TypeError("Invalid arguments passed to Sine")

###############################################################################

class Library:
    @staticmethod
    def saveSignal(signal, name):
        return _tact.Library_saveSignal(signal._handle, c_char_p(name.encode()))

    @staticmethod
    def loadSignal(name):
        handle = _tact.Library_loadSignal(c_char_p(name.encode()))
        if handle:
            return Signal(handle)
        return None

    @staticmethod
    def deleteSignal(name):
        return _tact.Library_deleteSignal(c_char_p(name.encode()))

    @staticmethod
    def exportSignal(signal, filePath, format=0, sampleRate=48000, maxLength=60):
        return _tact.Library_exportSignal(signal._handle, c_char_p(filePath.encoder()), format, sampleRate, maxLength)

    @staticmethod
    def importSignal(filePath, format=0, sampleRate=48000):
        handle = _tact.Library_importSignal(c_char_p(filePath.encode()), format, sampleRate)
        if handle:
            return Signal(handle)
        return None

###############################################################################

class Debug:
    @staticmethod
    def sigMapSize():
        return _tact.Debug_sigMapSize()

###############################################################################
# DLL IMPORTS
###############################################################################

Handle = c_void_p

def lib_func(func, restype, argtypes):
    func.restype = restype
    func.argtypes = argtypes 

# Session

lib_func(_tact.Session_create, Handle, None)
lib_func(_tact.Session_delete, None, [Handle])
lib_func(_tact.Session_open1, c_int, [Handle])
lib_func(_tact.Session_open2, c_int, [Handle, c_int])
lib_func(_tact.Session_open3, c_int, [Handle, c_int, c_int, c_double])
lib_func(_tact.Session_close, c_int, [Handle])
lib_func(_tact.Session_isOpen, c_bool, [Handle])

lib_func(_tact.Session_play, c_int, [Handle, c_int, Handle])
lib_func(_tact.Session_playAll, c_int, [Handle, Handle])
lib_func(_tact.Session_stop, c_int, [Handle, c_int])
lib_func(_tact.Session_stopAll, c_int, [Handle])
lib_func(_tact.Session_pause, c_int, [Handle, c_int])
lib_func(_tact.Session_pauseAll, c_int, [Handle])
lib_func(_tact.Session_resume, c_int, [Handle, c_int])
lib_func(_tact.Session_resumeAll, c_int, [Handle])
lib_func(_tact.Session_isPlaying, c_bool, [Handle, c_int])
lib_func(_tact.Session_isPaused, c_bool, [Handle, c_int])

lib_func(_tact.Session_setVolume, c_int, [Handle, c_int, c_double])
lib_func(_tact.Session_getVolume, c_double, [Handle, c_int])
lib_func(_tact.Session_setPitch, c_int, [Handle, c_int, c_double])
lib_func(_tact.Session_getPitch, c_double, [Handle, c_int])
lib_func(_tact.Session_getChannelCount, c_int, [Handle])
lib_func(_tact.Session_getSampleRate, c_double, [Handle])
lib_func(_tact.Session_getCpuLoad, c_double, [Handle])

lib_func(_tact.Session_count, c_int, None)

# Signal

lib_func(_tact.Signal_delete, None, [Handle])
lib_func(_tact.Signal_valid, c_bool, [Handle])
lib_func(_tact.Signal_sample, c_double, [Handle, c_double])
lib_func(_tact.Signal_length, c_double, [Handle])
lib_func(_tact.Signal_setGain, None, [Handle, c_double])
lib_func(_tact.Signal_getGain, c_double, [Handle])
lib_func(_tact.Signal_setBias, None, [Handle, c_double])
lib_func(_tact.Signal_getBias, c_double, [Handle])
lib_func(_tact.Signal_count, c_int, None)

# Operators

lib_func(_tact.Product_create, Handle, [Handle, Handle])
lib_func(_tact.Sum_create, Handle, [Handle, Handle])

lib_func(_tact.Mul_SigFlt, Handle, [Handle, c_double])
lib_func(_tact.Mul_FltSig, Handle, [c_double, Handle])

lib_func(_tact.Add_SigFlt, Handle, [Handle, c_double])
lib_func(_tact.Add_FltSig, Handle, [c_double, Handle])

lib_func(_tact.Sub_SigFlt, Handle, [Handle, c_double])
lib_func(_tact.Sub_FltSig, Handle, [c_double, Handle])

lib_func(_tact.Neg_Sig, Handle, [Handle])

# Sequence

lib_func(_tact.Sequence_create, Handle, None)
lib_func(_tact.Sequence_getHead, c_double, [Handle])
lib_func(_tact.Sequence_setHead, None, [Handle, c_double])
lib_func(_tact.Sequence_pushFlt, None, [Handle, c_double])
lib_func(_tact.Sequence_pushSig, None, [Handle, Handle])
lib_func(_tact.Sequence_pushSeq, None, [Handle, Handle])
lib_func(_tact.Sequence_insertSig, None, [Handle, Handle, c_double])
lib_func(_tact.Sequence_insertSeq, None, [Handle, Handle, c_double])
lib_func(_tact.Sequence_clear, None, [Handle])

lib_func(_tact.Sequence_SigSig, Handle, [Handle, Handle])
lib_func(_tact.Sequence_SigFlt, Handle, [Handle, c_double])
lib_func(_tact.Sequence_FltSig, Handle, [c_double, Handle])
lib_func(_tact.Sequence_SeqFlt, None, [Handle, c_double])
lib_func(_tact.Sequence_SeqSig, None, [Handle, Handle])
lib_func(_tact.Sequence_SeqSeq, None, [Handle, Handle])

# General

lib_func(_tact.Time_create, Handle, None)
lib_func(_tact.Scalar_create, Handle, [c_double])
lib_func(_tact.Ramp_create1, Handle, [c_double, c_double])
lib_func(_tact.Ramp_create2, Handle, [c_double, c_double, c_double])
lib_func(_tact.Noise_create, Handle, None)
lib_func(_tact.Expression_create, Handle, [c_char_p])
lib_func(_tact.Samples_create, Handle, [POINTER(c_float), c_int, c_double])

# Process

lib_func(_tact.Repeater_create, Handle, [Handle, c_int, c_double])
lib_func(_tact.Stretcher_create, Handle, [Handle, c_double])
lib_func(_tact.Reverser_create, Handle, [Handle])

# Envelope

lib_func(_tact.Envelope_create, Handle, [c_double, c_double])
lib_func(_tact.ASR_create, Handle, [c_double, c_double, c_double, c_double])
lib_func(_tact.ADSR_create, Handle, [c_double, c_double, c_double, c_double, c_double, c_double])

# Oscillator

lib_func(_tact.Sine_create1, Handle, [Handle])
lib_func(_tact.Sine_create2, Handle, [c_double])
lib_func(_tact.Sine_create3, Handle, [c_double, c_double])
lib_func(_tact.Sine_create4, Handle, [c_double, Handle, c_double])

lib_func(_tact.Square_create1, Handle, [Handle])
lib_func(_tact.Square_create2, Handle, [c_double])
lib_func(_tact.Square_create3, Handle, [c_double, c_double])
lib_func(_tact.Square_create4, Handle, [c_double, Handle, c_double])

lib_func(_tact.Saw_create1, Handle, [Handle])
lib_func(_tact.Saw_create2, Handle, [c_double])
lib_func(_tact.Saw_create3, Handle, [c_double, c_double])
lib_func(_tact.Saw_create4, Handle, [c_double, Handle, c_double])

lib_func(_tact.Triangle_create1, Handle, [Handle])
lib_func(_tact.Triangle_create2, Handle, [c_double])
lib_func(_tact.Triangle_create3, Handle, [c_double, c_double])
lib_func(_tact.Triangle_create4, Handle, [c_double, Handle, c_double])

lib_func(_tact.Pwm_create, Handle, [c_double, c_double])


# Library

lib_func(_tact.Library_saveSignal, c_bool, [Handle, c_char_p])
lib_func(_tact.Library_loadSignal, Handle, [c_char_p])
lib_func(_tact.Library_deleteSignal, c_bool, [c_char_p])
lib_func(_tact.Library_exportSignal, c_bool, [Handle, c_char_p, c_int, c_int, c_double])
lib_func(_tact.Library_importSignal, Handle, [c_char_p, c_int, c_int])

# Debug

lib_func(_tact.Debug_sigMapSize, int, None)

###############################################################################