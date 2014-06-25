/* reads user input keys and produces tinyfugue-keybinding macros */
/* written by a.sahlbach@earthbox.escape.de                       */

CALL RxFuncAdd 'SysGetKey', 'RexxUtil', 'SysGetKey'
keybuffer = ''
DO FOREVER
   SAY "--- Press keys and <cr> to produce a tf-keymacro with no istrip"
   SAY "--- Press only <cr> to end"
   CALL readbuffer
   IF Length(keybuffer) = 0 THEN LEAVE
   kmacro = ''
   i = 1
   DO WHILE i <= Length(keybuffer)
      key = SubStr(keybuffer,i,1)
      IF BitAnd(key,'80'x) = '80'x THEN
         kmacro = kmacro||'^['
      key = BitAnd(key,'7f'x)
      IF isctrl(key) THEN
         kmacro = kmacro||'^'ctrl(key)
      ELSE
         kmacro = kmacro||key
      i = i + 1
   END
   SAY "tf-macro for key-sequence "C2X(keybuffer)":"
   SAY " /def -b'"kmacro"' = <macro-action>"
END
EXIT 

ctrl:
   ret = BitAnd(C2D(Translate(Arg(1)))+C2D('@'),'7f'x)
RETURN D2C(ret)
   
isctrl: 
   knum = C2D(Arg(1))
   IF ((knum >= 0) & (knum <= 31)) | (knum >= 127) THEN
      ret = 1
   ELSE 
      ret = 0
RETURN ret

readbuffer:
   keybuffer = ""
   rkey = ""
   DO WHILE rkey <> D2C(13)
      IF rkey = X2C('E0') THEN rkey = D2C(0)
      keybuffer = keybuffer||rkey
      rkey = SysGetKey('NOECHO')
   END
   SAY ""
RETURN

