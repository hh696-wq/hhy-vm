# HHY Third-Party Notices

HHY dynamically links against libcurl, PCRE2 and BDWGC. The exact versions and linked
library paths for a release archive are recorded in that archive's `BUILD_INFO.txt`.

## libcurl

Copyright (C) Daniel Stenberg and many contributors. All rights reserved.

Permission to use, copy, modify, and distribute this software for any purpose with or
without fee is hereby granted, provided that the above copyright notice and this
permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of a copyright holder shall not be used
in advertising or otherwise to promote the sale, use or other dealings in this
Software without prior written authorization of the copyright holder.

## PCRE2

Copyright (c) 1997-2007 University of Cambridge  
Copyright (c) 2007-2024 Philip Hazel  
Copyright (c) 2009-2024 Zoltan Herczeg  
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notices, this list
   of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notices, this
   list of conditions and the following disclaimer in the documentation and/or other
   materials provided with the distribution.
3. Neither the name of the University of Cambridge nor the names of any contributors
   may be used to endorse or promote products derived from this software without
   specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

PCRE2 includes an exemption for binary library-like packages: the binary
redistribution condition does not propagate beyond a package that independently uses
PCRE2. Optional JIT components use the 2-clause BSD licence; HHY does not enable PCRE2
JIT in its Runtime.

## BDWGC

Copyright (c) 1988-1989 Hans-J. Boehm, Alan J. Demers  
Copyright (c) 1991-1996 Xerox Corporation. All rights reserved.  
Copyright (c) 1996-1999 Silicon Graphics. All rights reserved.  
Copyright (c) 1998 Fergus Henderson  
Copyright (c) 1999-2001 Red Hat, Inc.  
Copyright (c) 1999-2011 Hewlett-Packard Development Company, L.P.  
Copyright (c) 2004-2005 Andrei Polushin  
Copyright (c) 2007 Free Software Foundation, Inc.  
Copyright (c) 2008-2025 Ivan Maidanski  
Copyright (c) 2011 Ludovic Courtes  
Copyright (c) 2018 Petter A. Urkedal

THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED OR IMPLIED.
ANY USE IS AT YOUR OWN RISK.

Permission is hereby granted to use or copy this program for any purpose, provided the
above notices are retained on all copies. Permission to modify the code and to
distribute modified code is granted, provided the above notices are retained, and a
notice that the code was modified is included with the above copyright notice.

Several files in the upstream source distribution have slightly different permissive
licenses. HHY links the collector library supplied by the platform package and does
not redistribute BDWGC source or build-system files.
