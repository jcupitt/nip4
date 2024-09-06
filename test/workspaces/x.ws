<?xml version="1.0"?>
<root xmlns="http://www.vips.ecs.soton.ac.uk/nip/9.0.0">
  <Workspace view="WORKSPACE_MODE_REGULAR" scale="1" offset="0" locked="false" lpane_position="200" lpane_open="false" rpane_position="400" rpane_open="false" local_defs="// private definitions for this tab&#10;" name="tab2" filename="$HOME/GIT/nip4/test/workspaces/x.ws" major="9" minor="0">
    <Column x="5" y="5" open="true" selected="true" sform="false" next="22" name="A">
      <Subcolumn vislevel="3">
        <Row popup="false" name="A17">
          <Rhs vislevel="1" flags="4">
            <iText formula="2"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A18">
          <Rhs vislevel="1" flags="4">
            <iText formula="identity_matrix A17"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A16">
          <Rhs vislevel="1" flags="4">
            <iText formula="99"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A20">
          <Rhs vislevel="1" flags="4">
            <iText formula="A16 : (tl A18?0)"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A21">
          <Rhs vislevel="1" flags="4">
            <iText formula="A20 : (tl A18)"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A9">
          <Rhs vislevel="1" flags="4">
            <Subcolumn vislevel="0"/>
            <iText formula="Enum [&quot;Text&quot; =&gt; 0, &quot;Sliders&quot; =&gt; 1, &quot;Toggles&quot; =&gt; 2, &quot;Text plus scale and offset&quot; =&gt; 3]"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A10">
          <Rhs vislevel="1" flags="1">
            <iText formula="Option_enum &quot;Display as&quot; A9 $Text"/>
            <Option caption="Display as" labelsn="4" labels0="Text" labels1="Sliders" labels2="Toggles" labels3="Text plus scale and offset" value="3"/>
            <Subcolumn vislevel="0"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A11">
          <Rhs vislevel="4" flags="7">
            <Matrix/>
            <Subcolumn vislevel="2"/>
            <iText formula="Matrix_vips A21 1 0 &quot;&quot; A10.value"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A13">
          <Rhs vislevel="1" flags="4">
            <iText formula="A11.value"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A14">
          <Rhs vislevel="1" flags="4">
            <iText formula="A11.scale"/>
          </Rhs>
        </Row>
        <Row popup="false" name="A15">
          <Rhs vislevel="1" flags="4">
            <iText formula="A11.offset"/>
          </Rhs>
        </Row>
      </Subcolumn>
    </Column>
  </Workspace>
</root>
