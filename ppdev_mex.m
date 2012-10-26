% ppdev_mex() - Simple mex interface to ppdev
%
% Usage:
%   >> ppdev_mex(command)
%   >> portvalue = ppdev_mex(command, port)
%   >> ppdev_mex(command, port, value)
%   >> bitArray = ppdev_mex(command, port, value)
%
% Inputs:
%   command   - string 'Open', 'Close', 'CloseAll', or 'Write'
%   port      - double port number
%   value     - double scalar trigger value for 'Write'
%
% Outputs:
%   bitArray  - 1x16 logical vector pin state after 'WriteÄ
%
% Examples:
%   ppdev_mex('Open', 1);
%   ppdev_mex('Write', 1, 255);
%   WaitSecs(0.001);
%   ppdev_mex('Write', 1, 0);
%   ppdev_mex('Close', 1);
%   ppdev_mex('CloseAll');
%
% Note:
%   Based on ppMex.c by Eric Flister. The lp module should not be
%   loaded/unloaded for exclusive port access (Ubuntu: comment out the
%   respective line in /etc/modules). The user should have write access to
%   the port (Ubuntu: add user to the lp group in /etc/groups).
%
% Author: Andreas Widmann, University of Leipzig, 2012

%123456789012345678901234567890123456789012345678901234567890123456789012

% Copyright (C) 2012 Andreas Widmann, University of Leipzig, widmann@uni-leipzig.de
%
% This program is free software; you can redistribute it and/or modify it
% under the terms of the GNU General Public License as published by the
% Free Software Foundation; either version 2 of the License, or (at your
% option) any later version.
%
% This program is distributed in the hope that it will be useful, but
% WITHOUT ANY WARRANTY; without even the implied warranty of
% MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
% Public License for more details.
%
% You should have received a copy of the GNU General Public License along
% with this program; if not, write to the Free Software Foundation, Inc.,
% 59 Temple Place, Suite 330, Boston, MA  02111-1307, USA

function [ bitArray ] = ppdev_mex(command, port, value)
